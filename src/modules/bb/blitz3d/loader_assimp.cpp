
#include <bb/blitz/blitz.h>
#include "loader_assimp.h"
#include "meshmodel.h"
#include "meshutil.h"
#include "pivot.h"

#include <algorithm>
#include <map>

// #define SHOW_BONES

static bool conv,flip_tris;
static Transform conv_tform;
static bool collapse,animonly;

static std::map<std::string,Object*> obj_map;
static std::vector<Object*> bones;
static std::map<Object*,Transform> bone_offsets;
static MeshModel *skin_mesh;

inline Vector convertPos( aiVector3D &pos ){
	return Vector( pos.x,pos.y,-pos.z );
}

inline Vector convertVec( aiVector3D &scl ){
	return Vector( scl.x,scl.y,scl.z );
}

inline Quat convertQuat( aiQuaternion &rot ){
	// RH->LH (z negated) would be (w,-x,-y,z) in Hamilton convention, but
	// this engine's Quat multiplies in reverse order (geom.h), i.e. stores
	// the conjugate: (w,x,y,-z)
	return Quat( rot.w,Vector( rot.x,rot.y,-rot.z ) );
}

inline Transform convertTform( const aiMatrix4x4 &m ){
	// aiMatrix4x4 is row-major acting on column vectors; engine Matrix
	// holds columns i,j,k. RH->LH conjugation by S=diag(1,1,-1) negates
	// every element with exactly one z index
	Transform t;
	t.m.i=Vector( m.a1,m.b1,-m.c1 );
	t.m.j=Vector( m.a2,m.b2,-m.c2 );
	t.m.k=Vector( -m.a3,-m.b3,m.c3 );
	t.v=Vector( m.a4,m.b4,-m.c4 );
	return t;
}

inline void reset(){
	obj_map.clear();
	bones.clear();
	bone_offsets.clear();
	skin_mesh=0;
}

static Object *allocBone(){
#ifdef SHOW_BONES
	Brush b;
	b.setColor( Vector( 1,0,0 ) );
	b.setAlpha( .75f );
	MeshModel *bone=MeshUtil::createSphere( b,16 );
	Transform t;
	t.m.i.x=.4f;
	t.m.j.y=.4f;
	t.m.k.z=.4f;
	bone->transform( t );
#else
	Pivot *bone=d_new Pivot();
#endif

	return bone;
}

Object *Loader_Assimp::parseNode( const struct aiNode* nd,Object *parent ){
	MeshModel *mesh=0;
	Object *obj=0;
	std::string name=nd->mName.C_Str();

	if( nd->mNumMeshes>0 ){
		obj=mesh=d_new MeshModel();

		if( !animonly ){
			bool normals=true;
			bool skinned=false;
			MeshLoader::beginMesh();

			for( int n=0;n<nd->mNumMeshes;++n ){
				const struct aiMesh* mesh=scene->mMeshes[nd->mMeshes[n]];
				const aiMaterial *mtl=scene->mMaterials[mesh->mMaterialIndex];

				Brush b;

				aiColor4D diffuse;
				if( aiGetMaterialColor( mtl,AI_MATKEY_COLOR_DIFFUSE,&diffuse )==AI_SUCCESS ){
					b.setColor( Vector(diffuse.r,diffuse.g,diffuse.b) );
					if( diffuse.a ) b.setAlpha( diffuse.a );
				}

				for( int ti=0;ti<mtl->GetTextureCount( aiTextureType_DIFFUSE );ti++ ){
					aiString path;

					if( mtl->GetTexture( aiTextureType_DIFFUSE,ti,&path )==AI_SUCCESS ){
						if( const aiTexture *at=scene->GetEmbeddedTexture( path.C_Str() ) ){
							// GLB-style embedded texture: compressed (png/jpg)
							// when mHeight==0, raw BGRA texels otherwise
							if( at->mHeight==0 ){
								Texture tex( at->pcData,(size_t)at->mWidth,0 );
								b.setTexture( ti,tex,0 );
								b.setColor( Vector( 1,1,1 ) );
							}else{
								LOGD( "%s","uncompressed embedded textures are not supported in loader" );
							}
						}else{
							Texture tex( path.C_Str(),0 );
							b.setTexture( ti,tex,0 );
							b.setColor( Vector( 1,1,1 ) ); // TODO: this is what the existing loaders do...seems limiting...
						}
					}
				}

				int vertex_offset=MeshLoader::numVertices();

				for( int i=0;i<mesh->mNumVertices;++i ){
					Surface::Vertex v;

					v.coords=convertPos( mesh->mVertices[i] );
					if( mesh->mColors[0] ) {
						// v.color=Vector(&mesh->mColors[0][i]);
					}
					if( mesh->mNormals ){
						v.normal=convertPos( mesh->mNormals[i] );
					}else{
						normals=false;
					}
					if( mesh->HasTextureCoords(0) ){
						v.tex_coords[0][0]=mesh->mTextureCoords[0][i].x;
						v.tex_coords[0][1]=-mesh->mTextureCoords[0][i].y;
					}
					if( mesh->HasTextureCoords(1) ){
						v.tex_coords[1][0]=mesh->mTextureCoords[1][i].x;
						v.tex_coords[1][1]=-mesh->mTextureCoords[1][i].y;
					}
					MeshLoader::addVertex( v );
				}

				for( int t=0;t<mesh->mNumFaces;++t ) {
					const struct aiFace* face=&mesh->mFaces[t];

					// TODO: what to do here?
					if( face->mNumIndices!=3 ){
						// LOGD( "[loader.assimp] encountered face with %i indices\n",face->mNumIndices );
						continue;
					}

					int v0=face->mIndices[0],v1=face->mIndices[1],v2=face->mIndices[2];

					if( flip_tris ){
						MeshLoader::addTriangle( vertex_offset+v0,vertex_offset+v1,vertex_offset+v2,b );
					}else{
						MeshLoader::addTriangle( vertex_offset+v2,vertex_offset+v1,vertex_offset+v0,b );
					}
				}

				if( mesh->mNumBones ) skinned=true;

				for( int bi=0;bi<mesh->mNumBones;bi++ ){
					const aiBone *b=mesh->mBones[bi];

					Object *bone=obj_map[b->mName.C_Str()];
					if( bone==0 ){
						bone=allocBone();
						obj_map[b->mName.C_Str()]=bone;
					}

					// vertex bone indices are 1-based positions in `bones`
					// (slot 0 is reserved for the skinned mesh itself)
					int bone_index;
					std::vector<Object*>::iterator it=std::find( bones.begin(),bones.end(),bone );
					if( it==bones.end() ){
						bones.push_back( bone );
						bone_index=bones.size();
					}else{
						bone_index=int( it-bones.begin() )+1;
					}

					// the real inverse bind matrix: the node rest pose is not
					// guaranteed to match the skin's bind pose
					bone_offsets[bone]=convertTform( b->mOffsetMatrix );

					for( int wi=0;wi<b->mNumWeights;wi++ ){
						int vert=b->mWeights[wi].mVertexId;
						float weight=b->mWeights[wi].mWeight;
						MeshLoader::addBone( vertex_offset+vert,weight,bone_index );
					}
				}
			}

			MeshLoader::endMesh( mesh );
			if( !normals ) mesh->updateNormals();

			// remember which model holds the skinned vertices: the animator
			// must be bound to it, not to the scene root (glTF puts skinned
			// geometry in child nodes)
			if( skinned ){
				if( !skin_mesh ) skin_mesh=mesh;
				else LOGD( "%s","multiple skinned meshes in scene; only the first is animated" );
			}
		}
	}else{
		obj=obj_map[name];
		if( !obj ) obj=allocBone();
	}

	obj_map[name]=obj;

	aiVector3D pos, scl;
	aiQuaternion rot;
	nd->mTransformation.Decompose( scl,rot,pos );

	obj->setName( name );
	obj->setLocalPosition( convertPos( pos ) );
	obj->setLocalScale( convertVec( scl ) );
	obj->setLocalRotation( convertQuat( rot ) );

	for ( int n=0;n<nd->mNumChildren;++n ){
		parseNode( nd->mChildren[n],obj );
	}

	if( parent ){
		obj->setParent( parent );
	}else if( !mesh ) {
		mesh=d_new MeshModel();
		obj->setParent( mesh );
		obj=mesh;
	}

	return obj;
}

MeshModel *Loader_Assimp::load( const std::string &f,const Transform &t,int hint ){
	conv_tform=t;
	conv=flip_tris=false;
	if( conv_tform!=Transform() ){
		conv=true;
		if( conv_tform.m.i.cross(conv_tform.m.j).dot(conv_tform.m.k)<0 ) flip_tris=true;
	}

	// TODO: apply tforms...
	flip_tris=false;

	collapse=!!(hint&MeshLoader::HINT_COLLAPSE);
	animonly=!!(hint&MeshLoader::HINT_ANIMONLY);

	Assimp::Importer importer;
	scene=importer.ReadFile( f,aiProcessPreset_TargetRealtime_MaxQuality|aiProcess_Triangulate|aiProcess_JoinIdenticalVertices );
	if( !scene ) return 0;

	reset();

	Object *obj=parseNode( scene->mRootNode,0 );

	MeshModel *mesh=obj->getModel()->getMeshModel();

	if( !collapse ){
		// the animator harvests keys from a fixed set of objects, so every
		// channel-animated node must be in `bones` before it is created
		if( bones.size() ){
			for( int i=0;i<scene->mNumAnimations;i++ ){
				aiAnimation *a=scene->mAnimations[i];
				for( int j=0;j<a->mNumChannels;j++ ){
					Object *t=obj_map[a->mChannels[j]->mNodeName.C_Str()];
					if( t && std::count( bones.begin(),bones.end(),t )==0 ){
						bones.push_back( t );
					}
				}
			}
		}

		MeshModel *skin=skin_mesh ? skin_mesh : mesh;

		// each aiAnimation becomes one animator sequence: set the keys on the
		// nodes, then let the animator harvest them (addSeq clears them again)
		Animator *animator=0;
		for( int i=0;i<scene->mNumAnimations;i++ ){
			aiAnimation *a=scene->mAnimations[i];

			if( a->mNumMeshChannels ) LOGD( "%s","vertex-based animations are not supported in loader" );

			for( int j=0;j<a->mNumChannels;j++ ){
				aiNodeAnim *c=a->mChannels[j];

				Object *t=obj_map[c->mNodeName.C_Str()];
				if( !t ) continue;

				Animation anim=t->getAnimation();

				for( int k=0;k<c->mNumPositionKeys;k++ ){
					aiVectorKey key=c->mPositionKeys[k];
					anim.setPositionKey( key.mTime,convertPos( key.mValue ) );
				}

				for( int k=0;k<c->mNumRotationKeys;k++ ){
					aiQuatKey key=c->mRotationKeys[k];
					anim.setRotationKey( key.mTime,convertQuat( key.mValue ) );
				}

				for( int k=0;k<c->mNumScalingKeys;k++ ){
					aiVectorKey key=c->mScalingKeys[k];
					anim.setScaleKey( key.mTime,convertVec( key.mValue ) );
				}

				t->setAnimation( anim );
			}

			int anim_len=int( a->mDuration );

			if( animator ){
				animator->addSeq( anim_len );
			}else if( bones.size() ){
				bones.insert( bones.begin(),skin );
				animator=d_new Animator( bones,anim_len );
			}else{
				animator=d_new Animator( obj,anim_len );
			}
		}

		if( animator ){
			if( bones.size() ){
				skin->setAnimator( animator );
				skin->createBones( bone_offsets );
			}else{
				obj->setAnimator( animator );
			}
		}else if( skin && bones.size() ){
			// skinned but unanimated: bind anyway so bones can be driven
			// manually (FindChild+RotateEntity)
			bones.insert( bones.begin(),skin );
			skin->setAnimator( d_new Animator( bones,0 ) );
			skin->createBones( bone_offsets );
		}
		bones.clear();
	}

	reset();

	return mesh;
}
