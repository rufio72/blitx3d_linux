
#include "../stdutil/stdutil.h"
#include <bb/graphics.gl/graphics.gl.h>
#include <bb/system/system.h>
#include "blitz3d.gl.h"
#include "default.glsl.h"

#include <iostream>
#include <cmath>
#include <map>

//degrees to radians and back
static const float dtor=0.0174532925199432957692369076848861f;
static const float rtod=1/dtor;

#if defined(BB_MOBILE) || defined(BB_OVR) || defined(BB_NX)
#define GLES
#endif

class GLLight : public BBLightRep{
public:
	int type;

	float r,g,b;
	float matrix[16];

	float range;
	float inner_angle,outer_angle;

	void update( Light *light ){
		type=light->getType();

		r=light->getColor().x;
		g=light->getColor().y;
		b=light->getColor().z;

		range=light->getRange();
		light->getConeAngles( inner_angle,outer_angle );
		outer_angle*=rtod;

		const BBScene::Matrix *m=(BBScene::Matrix*)&(light->getRenderTform());

		matrix[ 0]=m->elements[0][0]; matrix[ 1]=m->elements[0][1]; matrix[ 2]=m->elements[0][2]; matrix[ 3]=0.0f;
		matrix[ 4]=m->elements[1][0]; matrix[ 5]=m->elements[1][1]; matrix[ 6]=m->elements[1][2]; matrix[ 7]=0.0f;
		matrix[ 8]=m->elements[2][0]; matrix[ 9]=m->elements[2][1]; matrix[10]=m->elements[2][2]; matrix[11]=0.0f;
		matrix[12]=m->elements[3][0]; matrix[13]=m->elements[3][1]; matrix[14]=m->elements[3][2]; matrix[15]=1.0f;
  }
};

struct GLVertex{
	float coords[3];
	float normal[3];
	float color[4];
	float tex_coord0[2];
	float tex_coord1[2];
};

class GLMesh : public BBMesh{
protected:
	bool dirty()const{ return false; }

public:
	int max_verts,max_tris,flags;
	GLVertex *verts=0;
	unsigned int *tris=0;

	unsigned int vertex_array=0;
	unsigned int vertex_buffer=0,index_buffer=0;

	GLMesh( int mv,int mt,int f ):max_verts(mv),max_tris(mt),flags(f){
		GL( glGenVertexArrays( 1,&vertex_array ) );
		GL( glGenBuffers( 1,&vertex_buffer ) );
		GL( glGenBuffers( 1,&index_buffer ) );

		GL( glBindVertexArray( vertex_array ) );

		GL( glBindBuffer( GL_ARRAY_BUFFER,vertex_buffer ) );
		GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER,index_buffer ) );

		GL( glEnableVertexAttribArray( 0 ) );
		GL( glEnableVertexAttribArray( 1 ) );
		GL( glEnableVertexAttribArray( 2 ) );
		GL( glEnableVertexAttribArray( 3 ) );
		GL( glEnableVertexAttribArray( 4 ) );

		GL( glVertexAttribPointer( 0,3,GL_FLOAT,GL_FALSE,sizeof( GLVertex ),(void*)offsetof( GLVertex,coords ) ) );
		GL( glVertexAttribPointer( 1,3,GL_FLOAT,GL_FALSE,sizeof( GLVertex ),(void*)offsetof( GLVertex,normal ) ) );
		GL( glVertexAttribPointer( 2,4,GL_FLOAT,GL_FALSE,sizeof( GLVertex ),(void*)offsetof( GLVertex,color ) ) );
		GL( glVertexAttribPointer( 3,2,GL_FLOAT,GL_FALSE,sizeof( GLVertex ),(void*)offsetof( GLVertex,tex_coord0 ) ) );
		GL( glVertexAttribPointer( 4,2,GL_FLOAT,GL_FALSE,sizeof( GLVertex ),(void*)offsetof( GLVertex,tex_coord1 ) ) );

		GL( glBindVertexArray( 0 ) );
	}

	~GLMesh(){
		delete[] verts;
		delete[] tris;
	}

	bool lock( bool all ){
		// TODO: this should probably come from a pool
		if( !verts ) verts=new GLVertex[max_verts];
		if( !tris ) tris=new unsigned int[max_tris*3];

		return true;
	}

	void unlock(){
		// Optimization #3: Upload both vertex and index buffers once in unlock()
		GL( glBindBuffer( GL_ARRAY_BUFFER,vertex_buffer ) );
		GL( glBufferData( GL_ARRAY_BUFFER,max_verts*sizeof(GLVertex),verts,GL_STATIC_DRAW ) );
		GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER,index_buffer ) );
		GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER,max_tris*3*sizeof(unsigned int),tris,GL_STATIC_DRAW ) );
	}

	void offsetIndices( int offset ){
		// Optimization #3: No longer re-uploads - data is already on GPU from unlock()
		// This function is now a no-op for non-GLES, offset is handled in glDrawElementsBaseVertex
#ifdef GLES
		GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER,index_buffer ) );
		GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER,(max_tris-offset)*3*sizeof(unsigned int),tris+offset*3,GL_STATIC_DRAW ) );
#endif
	}

	void offsetArrays( int offset ){
		// Optimization #3: Only needed for GLES which doesn't support glDrawElementsBaseVertex
#ifdef GLES
		GL( glBindBuffer( GL_ARRAY_BUFFER,vertex_buffer ) );
		GL( glBufferData( GL_ARRAY_BUFFER,(max_verts-offset)*sizeof(GLVertex),verts+offset,GL_STATIC_DRAW ) );
#endif
	}

	void setVertex( int n,const void *_v ){
		const Surface::Vertex *v=(const Surface::Vertex*)_v;
		float coords[3]={ v->coords.x,v->coords.y,v->coords.z };
		float normal[3]={ v->normal.x,v->normal.y,v->normal.z };
		setVertex( n,coords,normal,0xffffff,v->tex_coords );
	}

	void setVertex( int n,const float coords[3],const float normal[3],const float tex_coords[2][2] ){
		setVertex( n,coords,normal,0xffffff,tex_coords );
	}

	void setVertex( int n,const float coords[3],const float normal[3],unsigned argb,const float tex_coords[2][2] ){
		verts[n].coords[0]=coords[0];verts[n].coords[1]=coords[1];verts[n].coords[2]=coords[2];
		verts[n].normal[0]=normal[0];verts[n].normal[1]=normal[1];verts[n].normal[2]=normal[2];
		verts[n].tex_coord0[0]=tex_coords[0][0];verts[n].tex_coord0[1]=tex_coords[0][1];
		verts[n].tex_coord1[0]=tex_coords[1][0];verts[n].tex_coord1[1]=tex_coords[1][1];
		verts[n].color[0]=((argb>>16)&255)/255.0;verts[n].color[1]=((argb>>8)&255)/255.0;verts[n].color[2]=(argb&255)/255.0;verts[n].color[3]=((argb>>24)&255)/255.0;
	}

	void setTriangle( int n,int v0,int v1,int v2 ){
		tris[n*3+0]=v2;
		tris[n*3+1]=v1;
		tris[n*3+2]=v0;
	}
};

// TODO: merge this with GLLight...
struct LightState{
	struct LightData{
		float mat[16];
		float color[4];
		float direction[4]; // Optimization #8: Pre-computed light direction (w=0 for padding)
	} data[8];

	int lights_used;
};

struct UniformState{
	float ambient[4];
	float brush_color[4];
	float fog_color[4];

	struct GLTexState{
		float mat[16];
		int blend, sphere_map, flags, cube_map;
	} texs[8];

	float fog_range[2];

	int texs_used;
	int use_vertex_color;
	float brush_shininess;
	int fullbright;
	int fog_mode;
	int alpha_test;
};

class GLScene : public BBScene{
private:
	int context_width,context_height;
	bool wireframe;
	UniformState us={ 0 };
	GLuint defaultProgram=0;
	int viewport[4];

	std::vector<GLLight*> lights;

	float view_matrix[16];

	// Cached uniform locations (optimization #1)
	struct {
		GLint proj_matrix = -1;
		GLint view_matrix = -1;
		GLint world_matrix = -1;
		GLint normal_matrix = -1; // Optimization #4
	} uniform_locs;

	// Cached view matrix for normal matrix calculation (optimization #4)
	float cached_view_matrix[16];

	// Cached UBO handles (optimization #2)
	GLuint light_ubo = 0;
	GLuint render_ubo = 0;
	bool ubos_initialized = false;

	// Texture state cache (optimization #5)
	struct {
		GLuint bound_2d[MAX_TEXTURES] = {0};
		GLuint bound_cube[MAX_TEXTURES] = {0};
	} tex_cache;

	// Blend state cache (optimization #9)
	struct {
		int current_blend = -1;
	} blend_cache;

	// Cull face state cache (optimization #9)
	int cached_cull_enabled = -1; // -1 = unknown

	// Texture parameter cache (optimization #10)
	struct TexParamCache {
		GLenum mag_filter = 0;
		GLenum min_filter = 0;
		GLenum wrap_s = 0;
		GLenum wrap_t = 0;
	};
	std::map<GLuint, TexParamCache> tex_param_cache;

	// Last bound VAO (optimization #11)
	GLuint last_bound_vao = 0;

	void setLights(){
		LightState ls={ 0 };

		// FIXME: replace hardcoded '8' with proper hardware-backed value.
		for( unsigned long i=0;i<8;i++ ){
			if( i>=lights.size() ){
				break;
			}

			ls.lights_used++;

			memcpy( ls.data[i].mat,lights[i]->matrix,sizeof(ls.data[i].mat) );
			ls.data[i].color[0]=lights[i]->r;ls.data[i].color[1]=lights[i]->g;ls.data[i].color[2]=lights[i]->b;ls.data[i].color[3]=1.0;

			// Optimization #8: Pre-compute light direction on CPU
			// This replaces the expensive rotationMatrix() calculation in the vertex shader
			// The light direction is transformed by: viewMatrix * lightTform * rotation(x, 90deg) * (0,1,0)
			// Which simplifies to extracting and transforming the rotated Y axis of the light

			// Light transform matrix (column-major)
			const float* ltm = lights[i]->matrix;

			// Rotation by 90 degrees around X axis transforms Y to Z
			// So we need the Z column of the light transform (elements [8,9,10])
			float lz[3] = { ltm[8], ltm[9], ltm[10] };

			// Transform by view matrix (column-major): result = viewMatrix * lz
			float dir[3];
			dir[0] = cached_view_matrix[0]*lz[0] + cached_view_matrix[4]*lz[1] + cached_view_matrix[8]*lz[2];
			dir[1] = cached_view_matrix[1]*lz[0] + cached_view_matrix[5]*lz[1] + cached_view_matrix[9]*lz[2];
			dir[2] = cached_view_matrix[2]*lz[0] + cached_view_matrix[6]*lz[1] + cached_view_matrix[10]*lz[2];

			// Normalize
			float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
			if( len > 0.0001f ){
				ls.data[i].direction[0] = dir[0] / len;
				ls.data[i].direction[1] = dir[1] / len;
				ls.data[i].direction[2] = dir[2] / len;
			} else {
				ls.data[i].direction[0] = 0.0f;
				ls.data[i].direction[1] = 0.0f;
				ls.data[i].direction[2] = 1.0f;
			}
			ls.data[i].direction[3] = 0.0f; // padding for alignment

			float z=1.0f,w=0.0f;
			if( lights[i]->type!=Light::LIGHT_DISTANT ){
				z=0.0f;
				w=1.0f;
			}

			float pos[]={ 0.0,0.0,z,w };
			// glLightfv( GL_LIGHT0+i,GL_POSITION,pos );

			if( lights[i]->type!=Light::LIGHT_DISTANT ){
				float light_range[]={ 0.0f };
				float range[]={ lights[i]->range };
				// glLightfv( GL_LIGHT0+i,GL_CONSTANT_ATTENUATION,light_range );
				// glLightfv( GL_LIGHT0+i,GL_LINEAR_ATTENUATION,range );
			}

			if( lights[i]->type==Light::LIGHT_SPOT ){
				float dir[]={ 0.0f,0.0f,-1.0f };
				float outer[]={ lights[i]->outer_angle/2.0f };
				float exponent[]={ 10.0f };
				// glLightfv( GL_LIGHT0+i,GL_SPOT_DIRECTION,dir );
				// glLightfv( GL_LIGHT0+i,GL_SPOT_CUTOFF,outer );
				// glLightfv( GL_LIGHT0+i,GL_SPOT_EXPONENT,exponent );
			}
		}

		// Optimization #2: Use member UBO, allocate once, update with glBufferSubData
		if( light_ubo==0 ){
			GL( glGenBuffers( 1,&light_ubo ) );
			GL( glBindBuffer( GL_UNIFORM_BUFFER,light_ubo ) );
			GL( glBufferData( GL_UNIFORM_BUFFER,sizeof(ls),nullptr,GL_DYNAMIC_DRAW ) );
			GL( glBindBufferRange( GL_UNIFORM_BUFFER,1,light_ubo,0,sizeof(ls) ) );
		}else{
			GL( glBindBuffer( GL_UNIFORM_BUFFER,light_ubo ) );
		}

		GL( glBufferSubData( GL_UNIFORM_BUFFER,0,sizeof(ls),&ls ) );
		GL( glBindBuffer( GL_UNIFORM_BUFFER,0 ) );
	}

public:
	GLScene():wireframe(false){
		memset( &us,0,sizeof(UniformState) );

		const float MIDLEVEL[]={ 0.5f,0.5f,0.5f,1.0f };
		setAmbient( MIDLEVEL );
	}

	int  hwTexUnits(){ return MAX_TEXTURES; }
	int  gfxDriverCaps3D(){ return 110; }

	// intentionally left blank...
	void setWBuffer( bool enable ){}
	void setHWMultiTex( bool enable ){}
	void setDither( bool enable ){}

	void setAntialias( bool enable ){}
	void setWireframe( bool enable ){
		wireframe=enable;
	}
	void setFlippedTris( bool enable ){
		GL( glFrontFace( enable ? GL_CW : GL_CCW ) );
	}
	void setAmbient( const float rgb[3] ){
		us.ambient[0]=rgb[0];us.ambient[1]=rgb[1];us.ambient[2]=rgb[2];us.ambient[3]=1.0f;
	}

	void setAmbient2( const float rgb[3] ){
		setAmbient( rgb );
	}

	void setFogColor( const float rgb[3] ){
		us.fog_color[0]=rgb[0];us.fog_color[1]=rgb[1];us.fog_color[2]=rgb[2];us.fog_color[3]=1.0;
	}

	void setFogRange( float nr,float fr ){
		us.fog_range[0]=nr;us.fog_range[1]=fr;
	}

	void setFogMode( int mode ){
		us.fog_mode=mode;
	}

	void setZMode( int mode ){
		switch( mode ){
		case ZMODE_NORMAL:
			GL( glEnable( GL_DEPTH_TEST ) );
			GL( glDepthMask( GL_TRUE ) );
			break;
		case ZMODE_DISABLE:
			GL( glDisable( GL_DEPTH_TEST ) );
			GL( glDepthMask( GL_TRUE ) );
			break;
		case ZMODE_CMPONLY:
			GL( glEnable( GL_DEPTH_TEST ) );
			GL( glDepthMask( GL_FALSE ) );
			break;
		}
	}

	void setCanvas( int w,int h ){
		context_width=w;
		context_height=h;
	}

	void setViewport( int x,int y,int w,int h ){
		y=context_height-(h+y);

		GL( glViewport( x,y,w,h ) );
		GL( glScissor( x,y,w,h ) );
	}

	void setProj( const float matrix[16] ){
		// Optimization #1: Use cached uniform location
		GL( glUniformMatrix4fv( uniform_locs.proj_matrix,1,GL_FALSE,matrix ) );
	}

	void setOrthoProj( float nr,float fr,float nr_l,float nr_r,float nr_t,float nr_b ){
		float mat[16]={
			1.0,0.0,0.0,0.0,
			0.0,1.0,0.0,0.0,
			0.0,0.0,1.0,0.0,
			0.0,0.0,0.0,1.0
		};

		float w=nr_r-nr_l;
		float h=nr_b-nr_t;

		float W=2/w;
		float H=2/h;
		float Q=1/(fr-nr);
		mat[0]=W;
		mat[5]=H;
		mat[10]=Q;
		mat[11]=0;
		mat[14]=-Q*nr;
		mat[15]=1;

		setProj( mat );
	}

	void setPerspProj( float nr,float fr,float nr_l,float nr_r,float nr_t,float nr_b ){
		float mat[16]={
			1.0,0.0,0.0,0.0,
			0.0,1.0,0.0,0.0,
			0.0,0.0,1.0,0.0,
			0.0,0.0,0.0,1.0
		};

		mat[0] = (2.0f*nr) / (nr_r-nr_l);
		mat[5] = (2.0f*nr) / (nr_t-nr_b);
		mat[8] = (nr_r+nr_l) / (nr_r-nr_l);
		mat[9] = (nr_t+nr_b) / (nr_t-nr_b);
		mat[10] = -(fr+nr) / (fr-nr);
		mat[11] = -1.0f;
		mat[14] = -(2.0f*fr*nr) / (fr-nr);
		mat[15] = 0.0f;

		setProj( mat );
	}

	void setViewMatrix( const Matrix *matrix ){
		float mat[16]={
			1.0,0.0, 0.0,0.0,
			0.0,1.0, 0.0,0.0,
			0.0,0.0,-1.0,0.0,
			0.0,0.0, 0.0,1.0
		};

		if( matrix ){
			const Matrix *m=matrix;
			mat[ 0]=m->elements[0][0]; mat[ 1]=m->elements[0][1]; mat[ 2]=-m->elements[0][2];
			mat[ 4]=m->elements[1][0]; mat[ 5]=m->elements[1][1]; mat[ 6]=-m->elements[1][2];
			mat[ 8]=m->elements[2][0]; mat[ 9]=m->elements[2][1]; mat[10]=-m->elements[2][2];
			mat[12]=m->elements[3][0]; mat[13]=m->elements[3][1]; mat[14]=-m->elements[3][2];
		}

		// Optimization #4: Cache view matrix for normal matrix calculation
		memcpy( cached_view_matrix, mat, sizeof(cached_view_matrix) );

		// Optimization #1: Use cached uniform location
		GL( glUniformMatrix4fv( uniform_locs.view_matrix,1,GL_FALSE,mat ) );
	}

	void setWorldMatrix( const Matrix *matrix ){
		float mat[16]={
			1.0,0.0,0.0,0.0,
			0.0,1.0,0.0,0.0,
			0.0,0.0,1.0,0.0,
			0.0,0.0,0.0,1.0
		};

		if( matrix ){
			const Matrix *m=matrix;
			mat[ 0]=m->elements[0][0];  mat[ 1]=m->elements[0][1]; mat[ 2]=m->elements[0][2];
			mat[ 4]=m->elements[1][0];  mat[ 5]=m->elements[1][1]; mat[ 6]=m->elements[1][2];
			mat[ 8]=m->elements[2][0];  mat[ 9]=m->elements[2][1]; mat[10]=m->elements[2][2];
			mat[12]=m->elements[3][0];  mat[13]=m->elements[3][1]; mat[14]=m->elements[3][2];
		}

		// Optimization #1: Use cached uniform location
		GL( glUniformMatrix4fv( uniform_locs.world_matrix,1,GL_FALSE,mat ) );

		// Optimization #4: Calculate normal matrix on CPU (transpose of inverse of upper-left 3x3 of modelview)
		// ModelView = View * World
		float mv[16];
		// Multiply cached_view_matrix * mat (column-major)
		for( int i=0; i<4; i++ ){
			for( int j=0; j<4; j++ ){
				mv[i*4+j] = 0;
				for( int k=0; k<4; k++ ){
					mv[i*4+j] += cached_view_matrix[k*4+j] * mat[i*4+k];
				}
			}
		}

		// Extract upper-left 3x3 and compute inverse-transpose
		// For orthonormal matrices (rotation only), inverse-transpose = original
		// For general case, we compute the proper inverse-transpose
		float m3[9] = {
			mv[0], mv[1], mv[2],
			mv[4], mv[5], mv[6],
			mv[8], mv[9], mv[10]
		};

		// Compute determinant of 3x3
		float det = m3[0]*(m3[4]*m3[8] - m3[5]*m3[7])
		          - m3[1]*(m3[3]*m3[8] - m3[5]*m3[6])
		          + m3[2]*(m3[3]*m3[7] - m3[4]*m3[6]);

		if( fabs(det) > 0.0001f ){
			float invDet = 1.0f / det;
			// Compute inverse (already transposed for column-major layout)
			float normal_mat[9] = {
				(m3[4]*m3[8] - m3[5]*m3[7]) * invDet,
				(m3[5]*m3[6] - m3[3]*m3[8]) * invDet,
				(m3[3]*m3[7] - m3[4]*m3[6]) * invDet,
				(m3[2]*m3[7] - m3[1]*m3[8]) * invDet,
				(m3[0]*m3[8] - m3[2]*m3[6]) * invDet,
				(m3[1]*m3[6] - m3[0]*m3[7]) * invDet,
				(m3[1]*m3[5] - m3[2]*m3[4]) * invDet,
				(m3[2]*m3[3] - m3[0]*m3[5]) * invDet,
				(m3[0]*m3[4] - m3[1]*m3[3]) * invDet
			};
			GL( glUniformMatrix3fv( uniform_locs.normal_matrix,1,GL_FALSE,normal_mat ) );
		} else {
			// Fallback to identity if matrix is degenerate
			float identity[9] = { 1,0,0, 0,1,0, 0,0,1 };
			GL( glUniformMatrix3fv( uniform_locs.normal_matrix,1,GL_FALSE,identity ) );
		}
	}

	void setRenderState( const RenderState &rs ){
		if( rs.fx&FX_ALPHATEST && !(rs.fx&FX_VERTEXALPHA) ){
			us.alpha_test = 1;
		} else {
			us.alpha_test = 0;
		}

		if( rs.fx&FX_FLATSHADED ){
			// TODO
		} else {
			// TODO
		}

		int fog_mode=us.fog_mode;
		if( rs.fx&FX_NOFOG ){
			us.fog_mode = 0;
		}

		// Optimization #9: Cache cull face state
		int want_cull = (rs.fx&FX_DOUBLESIDED) ? 0 : 1;
		if( cached_cull_enabled != want_cull ){
			if( want_cull ){
				GL( glEnable( GL_CULL_FACE ) );
			}else{
				GL( glDisable( GL_CULL_FACE ) );
			}
			cached_cull_enabled = want_cull;
		}

		int blend=rs.blend;

		// glShadeModel( rs.fx&FX_FLATSHADED ? GL_FLAT : GL_SMOOTH );

		// TODO: sort this out for ES
#ifndef GLES
		if( rs.fx&FX_WIREFRAME||wireframe ){
			GL( glPolygonMode(GL_FRONT_AND_BACK,GL_LINE) );
		}else{
			GL( glPolygonMode(GL_FRONT_AND_BACK,GL_FILL) );
		}
#endif

		us.brush_color[0]=rs.color[0];us.brush_color[1]=rs.color[1];us.brush_color[2]=rs.color[2];us.brush_color[3]=rs.alpha;
		us.brush_shininess=rs.shininess;

		us.fullbright=rs.fx&FX_FULLBRIGHT;
		us.use_vertex_color=rs.fx&FX_VERTEXCOLOR;

		// Optimization #5: Track which textures need to be unbound instead of unbinding all
		// We'll unbind unused slots at the end based on what was actually used

		us.texs_used=0;
		for( int i=0;i<MAX_TEXTURES;i++ ){
			const RenderState::TexState &ts=rs.tex_states[i];

			if( !ts.canvas ){
				// Optimization #5: Only unbind if something was previously bound
				if( tex_cache.bound_2d[i] != 0 ){
					GL( glActiveTexture( GL_TEXTURE0+i ) );
					GL( glBindTexture( GL_TEXTURE_2D,0 ) );
					tex_cache.bound_2d[i] = 0;
				}
				if( tex_cache.bound_cube[i] != 0 ){
					GL( glActiveTexture( GL_TEXTURE0+MAX_TEXTURES+i ) );
					GL( glBindTexture( GL_TEXTURE_CUBE_MAP,0 ) );
					tex_cache.bound_cube[i] = 0;
				}
				continue;
			}

			GLCanvas *canvas=(GLCanvas*)ts.canvas;

			if( !canvas ){
				// ---
			} else {
				int flags=ts.canvas->getFlags();

				if( flags&BBCanvas::CANVAS_TEX_CUBE ){
					// Unbind 2D only if needed
					if( tex_cache.bound_2d[i] != 0 ){
						GL( glActiveTexture( GL_TEXTURE0+i ) );
						GL( glBindTexture( GL_TEXTURE_2D,0 ) );
						tex_cache.bound_2d[i] = 0;
					}
					GL( glActiveTexture( GL_TEXTURE0+MAX_TEXTURES+i ) );
					tex_cache.bound_cube[i] = canvas->texture;
				}else{
					// Unbind cube only if needed
					if( tex_cache.bound_cube[i] != 0 ){
						GL( glActiveTexture( GL_TEXTURE0+MAX_TEXTURES+i ) );
						GL( glBindTexture( GL_TEXTURE_CUBE_MAP,0 ) );
						tex_cache.bound_cube[i] = 0;
					}
					GL( glActiveTexture( GL_TEXTURE0+i ) );
					tex_cache.bound_2d[i] = canvas->texture;
				}

				canvas->bind();

				float mat[16]={
					1.0,0.0, 0.0,0.0,
					0.0,1.0, 0.0,0.0,
					0.0,0.0,-1.0,0.0,
					0.0,0.0, 0.0,1.0
				};

				const Matrix *m=ts.matrix;
				if( m ){
					mat[ 0]=m->elements[0][0]; mat[ 1]=m->elements[0][1]; mat[ 2]=-m->elements[0][2];
					mat[ 4]=m->elements[1][0]; mat[ 5]=m->elements[1][1]; mat[ 6]=-m->elements[1][2];
					mat[ 8]=m->elements[2][0]; mat[ 9]=m->elements[2][1]; mat[10]=-m->elements[2][2];
					mat[12]=m->elements[3][0]; mat[13]=m->elements[3][1]; mat[14]=-m->elements[3][2];
				}

				memcpy( us.texs[us.texs_used].mat,mat,sizeof(mat) );

				bool no_filter=flags&BBCanvas::CANVAS_TEX_NOFILTERING;
				bool mipmap=flags&BBCanvas::CANVAS_TEX_MIPMAP;

				// Optimization #10: Cache texture parameters - only set when changed
				GLenum want_mag = no_filter ? GL_NEAREST : GL_LINEAR;
				GLenum want_min = mipmap ? GL_LINEAR_MIPMAP_LINEAR : (no_filter ? GL_NEAREST : GL_LINEAR);
				GLenum want_wrap_s = (flags&BBCanvas::CANVAS_TEX_CLAMPU) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
				GLenum want_wrap_t = (flags&BBCanvas::CANVAS_TEX_CLAMPV) ? GL_CLAMP_TO_EDGE : GL_REPEAT;

				TexParamCache& tpc = tex_param_cache[canvas->texture];
				if( tpc.mag_filter != want_mag ){
					GL( glTexParameteri( canvas->target,GL_TEXTURE_MAG_FILTER,want_mag ) );
					tpc.mag_filter = want_mag;
				}
				if( tpc.min_filter != want_min ){
					GL( glTexParameteri( canvas->target,GL_TEXTURE_MIN_FILTER,want_min ) );
					tpc.min_filter = want_min;
				}
				if( tpc.wrap_s != want_wrap_s ){
					GL( glTexParameteri( canvas->target,GL_TEXTURE_WRAP_S,want_wrap_s ) );
					tpc.wrap_s = want_wrap_s;
				}
				if( tpc.wrap_t != want_wrap_t ){
					GL( glTexParameteri( canvas->target,GL_TEXTURE_WRAP_T,want_wrap_t ) );
					tpc.wrap_t = want_wrap_t;
				}

				if( flags&BBCanvas::CANVAS_TEX_ALPHA ){
					us.alpha_test=1;
				}

				us.texs[us.texs_used].blend=ts.blend;
				us.texs[us.texs_used].sphere_map=flags&BBCanvas::CANVAS_TEX_SPHERE?1:0;
				us.texs[us.texs_used].cube_map=flags&BBCanvas::CANVAS_TEX_CUBE?1:0;
				us.texs[us.texs_used].flags=ts.flags;

				us.texs_used++;
			}
		}

		// Optimization #9: Cache blend state - only change when needed
		if( blend != blend_cache.current_blend ){
			blend_cache.current_blend = blend;
			switch( blend ){
			default:case BLEND_REPLACE:
				GL( glDisable( GL_BLEND ) );
				break;
			case BLEND_ALPHA:
				GL( glEnable( GL_BLEND ) );
				GL( glBlendFunc( GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA ) );
				break;
			case BLEND_MULTIPLY:
				GL( glEnable( GL_BLEND ) );
				GL( glBlendFunc( GL_DST_COLOR,GL_ZERO ) );
				break;
			case BLEND_ADD:
				GL( glEnable( GL_BLEND ) );
				GL( glBlendFunc( GL_SRC_ALPHA,GL_ONE ) );
				break;
			}
		}

		// Optimization #2: Use member UBO, allocate once, update with glBufferSubData
		if( render_ubo==0 ){
			GL( glGenBuffers( 1,&render_ubo ) );
			GL( glBindBuffer( GL_UNIFORM_BUFFER,render_ubo ) );
			GL( glBufferData( GL_UNIFORM_BUFFER,sizeof(us),nullptr,GL_DYNAMIC_DRAW ) );
			GL( glBindBufferRange( GL_UNIFORM_BUFFER,2,render_ubo,0,sizeof(us) ) );
		}else{
			GL( glBindBuffer( GL_UNIFORM_BUFFER,render_ubo ) );
		}

		GL( glBufferSubData( GL_UNIFORM_BUFFER,0,sizeof(us),&us ) );
		GL( glBindBuffer( GL_UNIFORM_BUFFER,0 ) );

		// restore
		us.fog_mode=fog_mode;
	}

	//rendering
	bool begin( const std::vector<BBLightRep*> &l ){
		if( !glIsProgram( defaultProgram ) ){
			GL( glUseProgram( 0 ) );

			// LOGD( "rebuilding shader...\n" );

			std::string src( DEFAULT_GLSL,DEFAULT_GLSL+DEFAULT_GLSL_SIZE );
			defaultProgram=_bbGLCompileProgram( "default.glsl",src );
			if( !defaultProgram ){
				RTEX( "Failed to compile shader" );
			}

			GL( glUseProgram( defaultProgram ) );

			for( int i=0;i<MAX_TEXTURES;i++ ){
				char sampler_name[20];
				snprintf( sampler_name,sizeof(sampler_name),"bbTexture[%i]",i );
				GLint texLocation=GL( glGetUniformLocation( defaultProgram,sampler_name ) );
				GL( glUniform1i( texLocation,i ) );
			}

			for( int i=0;i<MAX_TEXTURES;i++ ){
				char sampler_name[25];
				snprintf( sampler_name,sizeof(sampler_name),"bbTextureCube[%i]",i );
				GLint texLocation=GL( glGetUniformLocation( defaultProgram,sampler_name ) );
				GL( glUniform1i( texLocation,MAX_TEXTURES+i ) );
			}

			GLint lightIdx=GL( glGetUniformBlockIndex( defaultProgram,"BBLightState" ) );
			GL( glUniformBlockBinding( defaultProgram,lightIdx,1 ) );

			GLint renderIdx=GL( glGetUniformBlockIndex( defaultProgram,"BBRenderState" ) );
			GL( glUniformBlockBinding( defaultProgram,renderIdx,2 ) );

			// Optimization #1: Cache uniform locations once
			uniform_locs.proj_matrix = GL( glGetUniformLocation( defaultProgram,"bbProjMatrix" ) );
			uniform_locs.view_matrix = GL( glGetUniformLocation( defaultProgram,"bbViewMatrix" ) );
			uniform_locs.world_matrix = GL( glGetUniformLocation( defaultProgram,"bbWorldMatrix" ) );
			uniform_locs.normal_matrix = GL( glGetUniformLocation( defaultProgram,"bbNormalMatrix" ) );
		}

		GL( glUseProgram( defaultProgram ) );

		GL( glEnable( GL_SCISSOR_TEST ) );
		GL( glEnable( GL_CULL_FACE ) );

		GL( glDepthFunc( GL_LEQUAL ) );

		lights.clear();
		for( unsigned long i=0;i<l.size();i++ ) lights.push_back( dynamic_cast<GLLight*>(l[i]) );

		setLights();

		GL( glGetIntegerv( GL_VIEWPORT,viewport ) );

		return true;
	}

	void clear( const float rgb[3],float alpha,float z,bool clear_argb,bool clear_z ){
		if( !clear_argb && !clear_z ) return;

		GL( glClearColor( rgb[0],rgb[1],rgb[2],alpha ) );
		GL( glClearDepthf( z ) );
		GL( glClear( (clear_argb?GL_COLOR_BUFFER_BIT:0)|(clear_z?GL_DEPTH_BUFFER_BIT:0)  ) );
	}

	void render( BBMesh *m,int first_vert,int vert_cnt,int first_tri,int tri_cnt ){
		GLMesh *mesh=(GLMesh*)m;

		// Optimization #11: Only bind VAO if different from last one
		if( last_bound_vao != mesh->vertex_array ){
			GL( glBindVertexArray( mesh->vertex_array ) );
			last_bound_vao = mesh->vertex_array;
		}
#ifdef GLES
		// GLES doesn't support glDrawElementsBaseVertex, so we need to re-upload with offset
		mesh->offsetIndices( first_tri );
		mesh->offsetArrays( first_vert );
		GL( glDrawElements( GL_TRIANGLES,tri_cnt*3,GL_UNSIGNED_INT,0 ) );
#else
		// Optimization #3: Use offset in draw call instead of re-uploading buffers
		GL( glDrawElementsBaseVertex( GL_TRIANGLES,tri_cnt*3,GL_UNSIGNED_INT,
			(void*)(first_tri*3*sizeof(unsigned int)),first_vert ) );
#endif
		// Optimization #11: Don't unbind VAO - next render() will bind a new one anyway
	}

	void end(){
		GL( glUseProgram( 0 ) );
		GL( glActiveTexture( GL_TEXTURE0 ) );
		GL( glDisable( GL_DEPTH_TEST ) );

		GL( glDisable( GL_BLEND ) );

		// Optimization #11: Unbind VAO and reset cache at end of frame
		if( last_bound_vao != 0 ){
			GL( glBindVertexArray( 0 ) );
			last_bound_vao = 0;
		}

		// Reset state caches for next frame
		blend_cache.current_blend = -1;
		cached_cull_enabled = -1;

		GL( glViewport( viewport[0],viewport[1],viewport[2],viewport[3] ) );
		GL( glScissor( viewport[0],viewport[1],viewport[2],viewport[3] ) );
	}

	//lighting
	BBLightRep *createLight( int flags ){
		return d_new GLLight();
	}
	void freeLight( BBLightRep *l ){}

	//meshes
	BBMesh *createMesh( int max_verts,int max_tris,int flags ){
		BBMesh *mesh=d_new GLMesh( max_verts,max_tris,flags );
		mesh_set.insert( mesh );
		return mesh;
	}

	//info
	int getTrianglesDrawn()const{ return 0; }
};

BBScene *GLB3DGraphics::createScene( int w,int h,float d,int flags ){
	if( scene_set.size() ) return 0;

	GLScene *scene=d_new GLScene();
	scene_set.insert( scene );
	return scene;
}

BBMODULE_CREATE( blitz3d_gl ){
	return true;
}

BBMODULE_DESTROY( blitz3d_gl ){
	return true;
}
