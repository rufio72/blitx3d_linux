
#include "std.h"
#include "sprite.h"
#include "scene.h"

extern float stats3d[];

static float null[]={0,0,0};

static float tex_coords0[2][2]={ {0,0},{0,0} };
static float tex_coords1[2][2]={ {1,0},{1,0} };
static float tex_coords2[2][2]={ {1,1},{1,1} };
static float tex_coords3[2][2]={ {0,1},{0,1} };

static BBMesh *mesh;
static int mesh_size;
static std::vector<int> mesh_indices;

//Coalesced-draw state: order-independent sprites are collected during a render
//pass and flushed as one draw per brush. Opaque (BLEND_REPLACE) sprites flush in
//the opaque phase; additive (BLEND_ADD) sprites flush in the transparent phase.
//Both are safe to reorder: opaque relies on the depth buffer, additive blending
//is commutative.
//
//Quads are bucketed by brush (one bucket per distinct render state) so the brush
//is copied once per bucket rather than once per sprite, and no per-frame sort is
//needed. Buckets and their quad vectors are kept across frames and only their
//lengths reset, so steady state does no allocation.
struct SpriteQuad{
	Vector verts[4];
	bool reflected;
};
struct SpriteBucket{
	Brush brush;
	std::vector<SpriteQuad> quads;
};
//heap-allocated (never destroyed): the buckets retain Brush objects across
//frames, which must not run their ref-counted destructors during static teardown
//after the rest of the engine is already gone.
static std::vector<SpriteBucket> *buckets_opaque=new std::vector<SpriteBucket>();
static std::vector<SpriteBucket> *buckets_add=new std::vector<SpriteBucket>();
static int used_opaque,used_add;	//active buckets this pass
static BBMesh *batch_mesh;
static int batch_capacity;	//capacity in quads

static void addToBatch( std::vector<SpriteBucket> &buckets,int &used,
	const Brush &brush,const Vector verts[4],bool reflected ){
	int b=0;
	for( ;b<used;++b ){
		const Brush &bb=buckets[b].brush;
		if( !(bb<brush) && !(brush<bb) ) break;	//same render state
	}
	if( b==used ){
		if( used==(int)buckets.size() ) buckets.push_back( SpriteBucket() );
		buckets[b].brush=brush;
		buckets[b].quads.clear();
		++used;
	}
	SpriteQuad q;
	q.verts[0]=verts[0];q.verts[1]=verts[1];
	q.verts[2]=verts[2];q.verts[3]=verts[3];
	q.reflected=reflected;
	buckets[b].quads.push_back( q );
}

//Pack all buckets contiguously into the batch mesh and emit one draw per bucket.
//Triangle indices are absolute into the batch buffer so a bucket draws with
//baseVertex 0.
static void flushSpriteBatch( std::vector<SpriteBucket> &buckets,int used ){
	int total=0;
	for( int b=0;b<used;++b ) total+=(int)buckets[b].quads.size();
	if( !total ) return;

	if( total>batch_capacity ){
		if( batch_mesh ) bbScene->freeMesh( batch_mesh );
		batch_capacity=total+256;
		batch_mesh=bbScene->createMesh( batch_capacity*4,batch_capacity*2,0 );
	}

	batch_mesh->lock( false );
	int q=0;
	for( int b=0;b<used;++b ){
		const std::vector<SpriteQuad> &quads=buckets[b].quads;
		for( size_t i=0;i<quads.size();++i,++q ){
			const SpriteQuad &e=quads[i];
			int fv=q*4,ft=q*2;
			batch_mesh->setVertex( fv+0,&e.verts[0].x,null,tex_coords0 );
			batch_mesh->setVertex( fv+1,&e.verts[1].x,null,tex_coords1 );
			batch_mesh->setVertex( fv+2,&e.verts[2].x,null,tex_coords2 );
			batch_mesh->setVertex( fv+3,&e.verts[3].x,null,tex_coords3 );
			if( e.reflected ){
				batch_mesh->setTriangle( ft+0,fv+0,fv+2,fv+1 );
				batch_mesh->setTriangle( ft+1,fv+0,fv+3,fv+2 );
			}else{
				batch_mesh->setTriangle( ft+0,fv+0,fv+1,fv+2 );
				batch_mesh->setTriangle( ft+1,fv+0,fv+2,fv+3 );
			}
		}
	}
	batch_mesh->unlock();

	bbScene->setWorldMatrix( 0 );	//batched quads are already in world space
	bbScene->setRenderBones( 0,0 );	//sprites are never skinned
	int q0=0;
	for( int b=0;b<used;++b ){
		int cnt=(int)buckets[b].quads.size();
		if( !cnt ) continue;
		bbScene->setRenderState( buckets[b].brush.getRenderState() );
		bbScene->render( batch_mesh,0,total*4,q0*2,cnt*2 );
		q0+=cnt;
	}
}

static int allocIndex(){
	if( !mesh_indices.size() ){
		if( mesh_size ) bbScene->freeMesh( mesh );
		for( int k=0;k<256;++k ){
			mesh_indices.push_back( mesh_size++ );
		}
		mesh=bbScene->createMesh( mesh_size*4,mesh_size*2,0 );
	}
	int n=mesh_indices.back();
	mesh_indices.pop_back();
	return n;
}

static void freeIndex( int n ){
	mesh_indices.push_back( n );
	if( mesh_indices.size()!=mesh_size ) return;
	bbScene->freeMesh( mesh );
	mesh_indices.clear();
	mesh_size=0;
}

Sprite::Sprite():
view_mode(VIEW_MODE_FREE),
xhandle(0),yhandle(0),
rot(0),xscale(1),yscale(1),captured(false){
	setRenderSpace( RENDER_SPACE_WORLD );
	mesh_index=allocIndex();
}

Sprite::Sprite( const Sprite &t ):
Model(t),
view_mode(t.view_mode),
xhandle(t.xhandle),yhandle(t.yhandle),
rot(t.rot),xscale(t.xscale),yscale(t.yscale),captured(false){
	mesh_index=allocIndex();
}

Sprite::~Sprite(){
	freeIndex( mesh_index );
}

void Sprite::setRotation( float angle ){
	rot=angle;
}

void Sprite::setScale( float x,float y ){
	xscale=x;yscale=y;
}

void Sprite::setHandle( float x,float y ){
	xhandle=x;yhandle=y;
}

void Sprite::setViewmode( int mode ){
	view_mode=mode;
}

void Sprite::capture(){
	Model::capture();
	r_rot=rot;
	r_xscale=xscale;
	r_yscale=yscale;
	captured=true;
}

bool Sprite::beginRender( float tween ){
	Model::beginRender( tween );
	if( tween==1 || !captured ){
		r_rot=rot;
		r_xscale=xscale;
		r_yscale=yscale;
	}else{
		r_rot=(rot-r_rot)*tween+r_rot;
		r_xscale=(xscale-r_xscale)*tween+r_xscale;
		r_yscale=(yscale-r_yscale)*tween+r_yscale;
	}
	return true;
}

bool Sprite::render( const RenderContext &rc ){

	Transform t=getRenderTform();

	if( view_mode==VIEW_MODE_FREE ){
		t.m=rc.getCameraTform().m;
	}else if( view_mode==VIEW_MODE_UPRIGHT ){
		t.m.k=rc.getCameraTform().m.k;t.m.orthogonalize();
	}else if( view_mode==VIEW_MODE_UPRIGHT2 ){
		t.m=yawMatrix( matrixYaw( rc.getCameraTform().m ) ) * t.m;
	}

	t.m=t.m * rollMatrix( r_rot ) * scaleMatrix( r_xscale,r_yscale,1 );

	static Vector verts[4];
	verts[0]=t * Vector( -1-xhandle, 1-yhandle,0 );
	verts[1]=t * Vector(  1-xhandle, 1-yhandle,0 );
	verts[2]=t * Vector(  1-xhandle,-1-yhandle,0 );
	verts[3]=t * Vector( -1-xhandle,-1-yhandle,0 );

	if( !rc.getWorldFrustum().cull( verts,4 ) ) return false;

	//Order-independent, unordered sprites are coalesced into one draw per brush:
	//opaque (depth buffer resolves order) and additive (commutative blending).
	//Alpha-blended or explicitly-ordered sprites keep the per-sprite path, where
	//draw order is semantically meaningful (back-to-front / EntityOrder).
	if( getOrder()==0 ){
		int blend=getRenderBrush().getBlend();
		if( blend==BBScene::BLEND_REPLACE ){
			addToBatch( *buckets_opaque,used_opaque,getRenderBrush(),verts,rc.isReflected() );
			return false;
		}
		if( blend==BBScene::BLEND_ADD ){
			addToBatch( *buckets_add,used_add,getRenderBrush(),verts,rc.isReflected() );
			return false;
		}
	}

	mesh->lock( false );
	int fv=mesh_index*4,ft=mesh_index*2;
	mesh->setVertex( fv+0,&verts[0].x,null,tex_coords0 );
	mesh->setVertex( fv+1,&verts[1].x,null,tex_coords1 );
	mesh->setVertex( fv+2,&verts[2].x,null,tex_coords2 );
	mesh->setVertex( fv+3,&verts[3].x,null,tex_coords3 );
	if( rc.isReflected() ){
		mesh->setTriangle( ft+0,0,2,1 );
		mesh->setTriangle( ft+1,0,3,2 );
	}else{
		mesh->setTriangle( ft+0,0,1,2 );
		mesh->setTriangle( ft+1,0,2,3 );
	}
	mesh->unlock();

	enqueue( mesh,fv,4,ft,2 );
	return false;
}

void Sprite::clearBatch(){
	//keep bucket/quad storage; addToBatch() reclaims buckets in index order
	used_opaque=0;
	used_add=0;
}

void Sprite::renderBatch(){
	flushSpriteBatch( *buckets_opaque,used_opaque );
}

void Sprite::renderAddBatch(){
	flushSpriteBatch( *buckets_add,used_add );
}
