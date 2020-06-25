#define GLM_ENABLE_EXPERIMENTAL

#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <vector>
#include <map>

#include <GL/glew.h>	//must be before glfw, because most header files we need are in glew
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Camera.h"

using namespace std;

#define GLM_FORCE_RADIANS
#define MAX_BONES 240
#define MAX_BONES_PER_VERTEX 4

static inline glm::mat4 mat4_cast(const aiMatrix4x4 &m) { return glm::transpose(glm::make_mat4(&m.a1)); }
static inline glm::quat quat_cast(const aiQuaternion &q) { return glm::quat(q.w, q.x, q.y, q.z); }

GLuint program;
GLuint light_program;
unsigned int cubeVAO;

//glm::vec3 lightPos = glm::vec3(10.0f, 0.0f, 10.0f);

Camera camera(glm::vec3(0.0f, 3.0f, 10.0f));
GLfloat angle_x = 0.0f;
GLfloat angle_y = 0.0f;
GLfloat angle_speed = 0.25f;
GLfloat lastX = 0, lastY = 0;

bool mouse_flag = true;
bool mouse_clicked = false;
bool keys_pressed[1024];

bool run_animation = false;
bool blinn_flag = false;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

// struct Texture
// {
// 	GLuint id;
// 	std::string type;
// 	aiString path;
// };

struct BoneMatrix
{
	glm::mat4x4 offset;
	glm::mat4x4 final_transform;
};

struct VertexBones
{
	GLuint ids[MAX_BONES_PER_VERTEX]; // max bones influced per vertex
	float weights[MAX_BONES_PER_VERTEX];

	VertexBones()
	{
		memset(ids, 0, sizeof(ids));
		memset(weights, 0, sizeof(weights));
	}

	void addBoneData(GLuint id, float weight)
	{
		for (int i = 0; i < MAX_BONES_PER_VERTEX; i++) {
			if (weights[i] == 0) {
				ids[i] = id;
				weights[i] = weight;
				return;
			}
		}
	}
};

class Mesh
{
public:
	Mesh() {;}

	Mesh(vector<Vertex> ver, vector<GLuint> ind, GLuint tex, vector<VertexBones> bone,const char* texture_file) {
		vertices.assign(ver.begin(), ver.end());
		indices.assign(ind.begin(), ind.end());
		texture = tex;
		bones_id_weight.assign(bone.begin(), bone.end());

		setup(texture_file);
	}

	void draw(GLuint program)
	{
		// // Bind appropriate textures
        // int diffuse = 1;
        // int specular = 1;

		// glActiveTexture(GL_TEXTURE0); // Active proper texture unit before binding
		// // Retrieve texture number (the N in diffuse_textureN)
		// stringstream ss;
		// string number;
		// string name = this->textures[i].type;
		
		// if( name == "texture_diffuse" )
		// {
		// 	ss << diffuseNr++; // Transfer GLuint to stream
		// }
		// else if( name == "texture_specular" )
		// {
		// 	ss << specularNr++; // Transfer GLuint to stream
		// }
		
		// number = ss.str( );
		// Now set the sampler to the correct texture unit
		// glUniform1i( glGetUniformLocation( shader.Program, ( name + number ).c_str( ) ), i );
		// And finally bind the texture
		//glBindTexture( GL_TEXTURE_2D, this->textures[i].id );
        
        // Also set each mesh's shininess property to a default value (if you want you could extend this to another mesh property and possibly change this value)
        // glUniform1f( glGetUniformLocation( shader.Program, "material.shininess" ), 16.0f );
        
        // Draw mesh
		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES,(int) indices.size( ), GL_UNSIGNED_INT, 0 );

        glBindVertexArray(0);
	}

	void release()
	{
		glDeleteVertexArrays(1, &vao);
		glDeleteTextures(1, &texture);
		glDeleteBuffers(1, &vbo_vertices);
		glDeleteBuffers(1, &vbo_bones);
		glDeleteBuffers(1, &ebo);	
	}

	// mini bmp loader written by HSU YOU-LUN
	static unsigned char *load_bmp(const char *bmp, unsigned int *width, unsigned int *height, unsigned short int *bits)
	{
		unsigned char *result = nullptr;
		FILE *fp = fopen(bmp, "rb");
		if (!fp)
			return nullptr;
		char type[2];
		unsigned int size, offset;
		// check for magic signature	
		fread(type, sizeof(type), 1, fp);
		if (type[0] == 0x42 || type[1] == 0x4d) {
			fread(&size, sizeof(size), 1, fp);
			// ignore 2 two-byte reversed fields
			fseek(fp, 4, SEEK_CUR);
			fread(&offset, sizeof(offset), 1, fp);
			// ignore size of bmpinfoheader field
			fseek(fp, 4, SEEK_CUR);
			fread(width, sizeof(*width), 1, fp);
			fread(height, sizeof(*height), 1, fp);
			// ignore planes field
			fseek(fp, 2, SEEK_CUR);
			fread(bits, sizeof(*bits), 1, fp);
			unsigned char *pos = result = new unsigned char[size - offset];
			fseek(fp, offset, SEEK_SET);
			while (size - ftell(fp)>0)
				pos += fread(pos, 1, size - ftell(fp), fp);
		}
		fclose(fp);
		return result;
	}

private:
	vector<Vertex> vertices;
	vector<GLuint> indices;
	GLuint texture;
	vector<VertexBones> bones_id_weight;

	GLuint vao;
	GLuint vbo_vertices;
	GLuint vbo_bones;
	GLuint ebo;

	void setup(const char* texbmp) {
		// VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// Verteices
		glGenBuffers(1, &vbo_vertices);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
			// Upload postion array
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*) 0);
        	// Vertex Normals
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*) offsetof(Vertex, normal));
        	// Vertex Texture Coords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*) offsetof(Vertex, texCoords));
        
		// Bones
		glGenBuffers(1, &vbo_bones);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_bones);
		glBufferData(GL_ARRAY_BUFFER, bones_id_weight.size() * sizeof(bones_id_weight[0]), &bones_id_weight[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(3);
		glVertexAttribIPointer(3, MAX_BONES_PER_VERTEX, GL_INT, sizeof(VertexBones), (GLvoid*) 0);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, MAX_BONES_PER_VERTEX, GL_FLOAT, GL_FALSE, sizeof(VertexBones), (GLvoid*) offsetof(VertexBones, weights));

		// Indices
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

        glBindVertexArray(0);

		// glGenTextures(1, &texture);

		// glBindTexture(GL_TEXTURE_2D, texture);
		// unsigned int width, height;
		// unsigned short int bits;
		// unsigned char *bgr = load_bmp(texbmp, &width, &height, &bits);
		// GLenum format = (bits == 24 ? GL_BGR : GL_BGRA);
		// glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, GL_UNSIGNED_BYTE, bgr);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		// glGenerateMipmap(GL_TEXTURE_2D);
		// delete[] bgr;
	}
};

class Model
{
public:
	Model() {;}

	Model(const string path)
	{
		loadModel(path);
	}

	void initBoneLoc(GLuint program)
	{
		for (int i = 0; i < MAX_BONES; i++) {
			string name = "bones[" + to_string(i) + "]";
			bone_location[i] = glGetUniformLocation(program, name.c_str());
		}
	}

	void loadModel(const std::string path)
	{
		scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
		if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::cout << "error assimp : " << importer.GetErrorString() << std::endl;
			return;
		}

		string filename = path.substr(0, path.find_last_of('.')) + ".bmp";
		texture_name = filename.c_str();
		cout << texture_name << endl;

		inverse_transform = mat4_cast(scene->mRootNode->mTransformation.Inverse());

		cout << "scene->HasAnimations(): " << scene->HasAnimations() << endl;
		if (scene->HasAnimations()) {
			if (scene->mAnimations[0]->mTicksPerSecond != 0.0) {
				ticks_per_second = scene->mAnimations[0]->mTicksPerSecond;
			} else {
				ticks_per_second = 25.0f;
			}
			cout << "scene->mAnimations[0]->mNumChannels: " << scene->mAnimations[0]->mNumChannels << endl;
			cout << "scene->mAnimations[0]->mDuration: " << scene->mAnimations[0]->mDuration << endl;
			cout << "scene->mAnimations[0]->mTicksPerSecond: " << scene->mAnimations[0]->mTicksPerSecond << endl << endl;
			cout << "name nodes animation : " << endl;
			for (uint i = 0; i < scene->mAnimations[0]->mNumChannels; i++) {
				cout<< scene->mAnimations[0]->mChannels[i]->mNodeName.C_Str() << endl;
			}
		}
		cout << endl;
		
		cout << "nodes:" << endl;
		showNode(scene->mRootNode);
		cout << endl;

		processMesh(scene);
	}

	void showNode(aiNode* node)
	{
		std::cout << node->mName.data << std::endl;
		for (int i = 0; i < node->mNumChildren; i++)
		{
			showNode(node->mChildren[i]);
		}
	}

	void render(GLuint program)
	{
		vector<glm::mat4x4> transforms;
		boneTransform((float)glfwGetTime(), transforms);
		
		for (int i = 0; i < transforms.size(); i++) {
			glUniformMatrix4fv(bone_location[i], 1, GL_FALSE, glm::value_ptr(transforms[i]));
		}
		
		for (int i = 0; i < meshes.size(); i++) {
            meshes[i].draw(program);
        }
	}

	void update()
	{
		// head
		if (keys_pressed[GLFW_KEY_1])
			rotate_head *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(-1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_2])
        	rotate_head *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(-1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_3])
			rotate_head *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_4])
			rotate_head *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		
		// body
		if (keys_pressed[GLFW_KEY_5])
			rotate_body *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(-1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_6])
        	rotate_body *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(-1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_7])
			rotate_body *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_8])
			rotate_body *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		
		// front left leg2
		if (keys_pressed[GLFW_KEY_R])
			rotate_fl_leg2 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_F])
			rotate_fl_leg2 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		// front left leg1
		if (keys_pressed[GLFW_KEY_T])
        	rotate_fl_leg1 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_G])
			rotate_fl_leg1 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		// front right leg1
		if (keys_pressed[GLFW_KEY_Y])
        	rotate_fr_leg1 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, -1.0f));
		if (keys_pressed[GLFW_KEY_H])
			rotate_fr_leg1 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, -1.0f));
		// front right leg2
		if (keys_pressed[GLFW_KEY_U])
        	rotate_fr_leg2 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, -1.0f));
		if (keys_pressed[GLFW_KEY_J])
			rotate_fr_leg2 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, -1.0f));
		
		// back left leg2
		if (keys_pressed[GLFW_KEY_I])
			rotate_bl_leg2 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_K])
			rotate_bl_leg2 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		// back left leg1
		if (keys_pressed[GLFW_KEY_O])
        	rotate_bl_leg1 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_L])
			rotate_bl_leg1 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		// back right leg1
		if (keys_pressed[GLFW_KEY_P])
        	rotate_br_leg1 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_SEMICOLON])
			rotate_br_leg1 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		// back right leg2
		if (keys_pressed[GLFW_KEY_LEFT_BRACKET])
        	rotate_br_leg2 *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_APOSTROPHE])
			rotate_br_leg2 *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, 1.0f));
		
		// tail
		if (keys_pressed[GLFW_KEY_Z])
			rotate_tail *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(1.0f, 0.0f, -1.0f));
		if (keys_pressed[GLFW_KEY_X])
        	rotate_tail *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(1.0f, 0.0f, -1.0f));
		if (keys_pressed[GLFW_KEY_C])
			rotate_tail *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(-1.0f, 0.0f, -1.0f));
		if (keys_pressed[GLFW_KEY_V])
			rotate_tail *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(-1.0f, 0.0f, -1.0f));

		//left wing
		if (keys_pressed[GLFW_KEY_B])
			rotate_left_wing *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(0.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_N])
        	rotate_left_wing *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(0.0f, 0.0f, 1.0f));
		//right wing
		if (keys_pressed[GLFW_KEY_M])
			rotate_right_wing *= glm::quat(cos(glm::radians(1.0f / 2)), sin(glm::radians(-1.0f / 2)) * glm::vec3(0.0f, 0.0f, 1.0f));
		if (keys_pressed[GLFW_KEY_COMMA])
        	rotate_right_wing *= glm::quat(cos(glm::radians(-1.0f / 2)), sin(glm::radians(1.0f / 2)) * glm::vec3(0.0f, 0.0f, 1.0f));
	}

	void releaseObject()
	{
		for (int i = 0; i < meshes.size(); i++){
			meshes[i].release();
		}
	}

private:
	const char* texture_name;
	Assimp::Importer importer;
	const aiScene* scene;
	std::vector<Mesh> meshes;
	map<string, GLuint> bone_mapping;
	GLuint num_bones = 0;
	vector<BoneMatrix> bone_matrices;
	GLuint bone_location[MAX_BONES];
	glm::mat4x4 inverse_transform;

	float ticks_per_second = 0;

	glm::quat rotate_head = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_body = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_fl_leg1 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_fl_leg2 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_fr_leg1 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_fr_leg2 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_bl_leg1 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_bl_leg2 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_br_leg1 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_br_leg2 = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_tail = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_left_wing = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotate_right_wing = glm::quat(cos(glm::radians(0.0f)), sin(glm::radians(0.0f)) * glm::vec3(1.0f, 0.0f, 0.0f));


	int findTranslation(float animationTime, const aiNodeAnim* pNodeAnim) {
        assert(pNodeAnim->mNumPositionKeys > 0);
        
        for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++) {
            if (animationTime < (float)pNodeAnim->mPositionKeys[i+1].mTime) {
                return i;
            }
        }
        
        return -1;
    }

	int findRotation(float animationTime, const aiNodeAnim* pNodeAnim) {
        assert(pNodeAnim->mNumRotationKeys > 0);
        
        for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
            if (animationTime < (float)pNodeAnim->mRotationKeys[i+1].mTime) {
                return i;
            }
        }
        return -1;
    }

	int findScaling(float animationTime, const aiNodeAnim* pNodeAnim) {
        assert(pNodeAnim->mNumScalingKeys > 0);
        
        for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1 ; i++) {
            if (animationTime < (float)pNodeAnim->mScalingKeys[i+1].mTime) {
                return i;
            }
        }
        
        return -1;
    }

	aiVector3D calcInterpolatedPosition(float animation_time, const aiNodeAnim* node_anim)
	{
		if (node_anim->mNumPositionKeys == 1) {
			return node_anim->mPositionKeys[0].mValue;
		}

		int position_index = findTranslation(animation_time, node_anim);
		int next_position_index = position_index + 1;
		assert(next_position_index < node_anim->mNumPositionKeys);
		float delta_time = (float)(node_anim->mPositionKeys[next_position_index].mTime - node_anim->mPositionKeys[position_index].mTime);
		float factor = (animation_time - (float)node_anim->mPositionKeys[position_index].mTime) / delta_time;
		//assert(factor >= 0.0f && factor <= 1.0f);

		aiVector3D start = node_anim->mPositionKeys[position_index].mValue;
		aiVector3D end = node_anim->mPositionKeys[next_position_index].mValue;
		aiVector3D delta = end - start;

		return start + factor * delta;
	}

	void calcInterpolatedRotation(aiQuaternion& quat, float animation_time, const aiNodeAnim* node_anim)
	{
		if (node_anim->mNumRotationKeys == 1) {
			quat = node_anim->mRotationKeys[0].mValue;
			return;
		}
		
		int rotation_index = findRotation(animation_time, node_anim);
		int next_rotation_index = rotation_index + 1;
		assert(next_rotation_index < node_anim->mNumRotationKeys);
		float delta_time = (float)(node_anim->mRotationKeys[next_rotation_index].mTime - node_anim->mRotationKeys[rotation_index].mTime);
		float factor = (animation_time - (float)node_anim->mRotationKeys[rotation_index].mTime) / delta_time;
		//assert(factor >= 0.0f && factor <= 1.0f);

		aiQuaternion start_quat = node_anim->mRotationKeys[rotation_index].mValue;
		aiQuaternion end_quat = node_anim->mRotationKeys[next_rotation_index].mValue;

		aiQuaternion::Interpolate(quat, start_quat, end_quat, factor);
	}

	aiVector3D calcInterpolatedScaling(float animation_time, const aiNodeAnim* node_anim)
	{
		if (node_anim->mNumScalingKeys == 1) {
			return node_anim->mScalingKeys[0].mValue;
		}

		int scaling_index = findScaling(animation_time, node_anim);

		int next_scaling_index = scaling_index + 1;
		assert(next_scaling_index < node_anim->mNumScalingKeys);
		float delta_time = node_anim->mScalingKeys[next_scaling_index].mTime - node_anim->mScalingKeys[scaling_index].mTime;
		float factor = (animation_time - node_anim->mScalingKeys[scaling_index].mTime) / delta_time;
		//assert(factor >= 0.0f && factor <= 1.0f);

		aiVector3D start = node_anim->mScalingKeys[scaling_index].mValue;
		aiVector3D end = node_anim->mScalingKeys[next_scaling_index].mValue;
		aiVector3D delta = end - start;

		return start + delta * factor;
	}

	const aiNodeAnim* findNodeAnim(const aiAnimation* animation, const string node_name)
	{
		for (int i = 0; i < animation->mNumChannels; i++) {
			const aiNodeAnim* node_anim = animation->mChannels[i];
			if (string(node_anim->mNodeName.data) == string(node_name)) {
				return node_anim;
			}
		}

		return nullptr;
	}

	void readNodeHierarchy(float animation_time, const aiNode* node, const glm::mat4x4& transform)
	{
		string node_name(node->mName.data);
		glm::mat4x4 node_transform = mat4_cast(node->mTransformation);

		if (scene->HasAnimations() && run_animation) {
			const aiAnimation* animation = scene->mAnimations[0];
			const aiNodeAnim* node_anim = findNodeAnim(animation, node_name);
			
			if (node_anim) {
				//scaling
				// cout << animation_time << endl;
				aiVector3D scaling_vector = calcInterpolatedScaling(animation_time, node_anim);
				glm::mat4x4 scaling_matr = glm::mat4x4(1.0f);
				scaling_matr = glm::scale(scaling_matr, glm::vec3(scaling_vector.x, scaling_vector.y, scaling_vector.z));
				
				//rotation
				aiQuaternion rotate_quat;
				calcInterpolatedRotation(rotate_quat, animation_time, node_anim);
				glm::mat4x4 rotate_matr = glm::toMat4(quat_cast(rotate_quat));

				//translation
				aiVector3D translate_vector = calcInterpolatedPosition(animation_time, node_anim);
				glm::mat4x4 translate_matr = glm::mat4x4(1.0f);
				translate_matr = glm::translate(translate_matr, glm::vec3(translate_vector.x, translate_vector.y, translate_vector.z));
				
				node_transform = translate_matr * rotate_matr * scaling_matr;
			}
		}

		glm::mat4x4 global_transform = transform * node_transform;

		if (bone_mapping.find(node_name) != bone_mapping.end()) {
			if (string(node->mName.data) == "Armature_Bone_head") {
				aiQuaternion rotate_h = aiQuaternion(rotate_head.w, rotate_head.x, rotate_head.y, rotate_head.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_body") {
				aiQuaternion rotate_h = aiQuaternion(rotate_body.w, rotate_body.x, rotate_body.y, rotate_body.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_front_left_leg1") {
				aiQuaternion rotate_h = aiQuaternion(rotate_fl_leg1.w, rotate_fl_leg1.x, rotate_fl_leg1.y, rotate_fl_leg1.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_front_left_leg2") {
				aiQuaternion rotate_h = aiQuaternion(rotate_fl_leg2.w, rotate_fl_leg2.x, rotate_fl_leg2.y, rotate_fl_leg2.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_front_right_leg1") {
				aiQuaternion rotate_h = aiQuaternion(rotate_fr_leg1.w, rotate_fr_leg1.x, rotate_fr_leg1.y, rotate_fr_leg1.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_front_right_leg2") {
				aiQuaternion rotate_h = aiQuaternion(rotate_fr_leg2.w, rotate_fr_leg2.x, rotate_fr_leg2.y, rotate_fr_leg2.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_back_left_leg1") {
				aiQuaternion rotate_h = aiQuaternion(rotate_bl_leg1.w, rotate_bl_leg1.x, rotate_bl_leg1.y, rotate_bl_leg1.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_back_left_leg2") {
				aiQuaternion rotate_h = aiQuaternion(rotate_bl_leg2.w, rotate_bl_leg2.x, rotate_bl_leg2.y, rotate_bl_leg2.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_back_right_leg1") {
				aiQuaternion rotate_h = aiQuaternion(rotate_br_leg1.w, rotate_br_leg1.x, rotate_br_leg1.y, rotate_br_leg1.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_back_right_leg2") {
				aiQuaternion rotate_h = aiQuaternion(rotate_br_leg2.w, rotate_br_leg2.x, rotate_br_leg2.y, rotate_br_leg2.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_tail") {
				aiQuaternion rotate_h = aiQuaternion(rotate_tail.w, rotate_tail.x, rotate_tail.y, rotate_tail.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_left_wing") {
				aiQuaternion rotate_h = aiQuaternion(rotate_left_wing.w, rotate_left_wing.x, rotate_left_wing.y, rotate_left_wing.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			} else if (string(node->mName.data) == "Armature_Bone_right_wing") {
				aiQuaternion rotate_h = aiQuaternion(rotate_right_wing.w, rotate_right_wing.x, rotate_right_wing.y, rotate_right_wing.z);
				global_transform *= glm::toMat4(quat_cast(rotate_h));
			}
			int bone_index = bone_mapping[node_name];
			bone_matrices[bone_index].final_transform = inverse_transform * global_transform * bone_matrices[bone_index].offset;
		}

		for (int i = 0; i < node->mNumChildren; i++) {
			readNodeHierarchy(animation_time, node->mChildren[i], global_transform);
		}
	}

	void boneTransform(float secs, vector<glm::mat4x4>& transforms)
	{
		glm::mat4 identity_matrix(1.0f);

		float ticks = secs * ticks_per_second;
		float animation_time = 0;
		if (scene->HasAnimations()) {
			animation_time = fmod(ticks, scene->mAnimations[0]->mDuration);
		}

		if (run_animation) {
			readNodeHierarchy(animation_time, scene->mRootNode, identity_matrix);
		} else {
			readNodeHierarchy(0, scene->mRootNode, identity_matrix);
		}
		
		transforms.resize(num_bones);

		for (int i = 0; i < num_bones; i++) {
			transforms[i] = bone_matrices[i].final_transform;
		}
	}

	void processMesh(const aiScene* scene)
	{
		Mesh mesh;
		for (int i = 0; i < scene->mNumMeshes; i++) {
			aiMesh* ai_mesh = scene->mMeshes[i];

			vector<Vertex> vertices;
			vector<GLuint> indices;
			vector<VertexBones> bones_id_weight;
			GLuint texture;

			vertices.reserve(ai_mesh->mNumVertices);
			indices.reserve(ai_mesh->mNumVertices);
			bones_id_weight.resize(ai_mesh->mNumVertices);

			for (int i = 0; i < ai_mesh->mNumVertices; i++) {
				Vertex vertex;
				glm::vec3 vector;

				vector.x = ai_mesh->mVertices[i].x;
				vector.y = ai_mesh->mVertices[i].y;
				vector.z = ai_mesh->mVertices[i].z;
				vertex.position = vector;

				vector.x = ai_mesh->mNormals[i].x;
				vector.y = ai_mesh->mNormals[i].y;
				vector.z = ai_mesh->mNormals[i].z;
				vertex.normal = vector;

				if( ai_mesh->mTextureCoords[0] ) {
					glm::vec2 vec;
					vec.x = ai_mesh->mTextureCoords[0][i].x;
					vec.y = ai_mesh->mTextureCoords[0][i].y;
					vertex.texCoords = vec;
				} else {
					vertex.texCoords = glm::vec2( 0.0f, 0.0f );
				}
				
				vertices.push_back(vertex);
			}

			for (int i = 0; i < ai_mesh->mNumFaces; i++) {
				aiFace face = ai_mesh->mFaces[i];
				for(unsigned int j = 0; j < face.mNumIndices; j++)
            		indices.push_back(face.mIndices[j]);
			}

			for (int i = 0; i < ai_mesh->mNumBones; i++) {
				int bone_index = 0;
				string bone_name(ai_mesh->mBones[i]->mName.data);

				// cout << ai_mesh->mBones[i]->mName.data << endl;

				if (bone_mapping.find(bone_name) == bone_mapping.end()) {
					// Allocate an index for a new bone
					bone_index = num_bones;
					num_bones++;
					BoneMatrix bone_matrix;
					bone_matrix.offset = mat4_cast(ai_mesh->mBones[i]->mOffsetMatrix);
					bone_matrices.push_back(bone_matrix);
					bone_mapping[bone_name] = bone_index;

					// cout << "bone_name: " << bone_name << endl;
					// cout << "bone_index: " << bone_index << endl;
				} else {
					bone_index = bone_mapping[bone_name];
				}

				for (int j = 0; j < ai_mesh->mBones[i]->mNumWeights; j++) {
					GLuint vertex_id = ai_mesh->mBones[i]->mWeights[j].mVertexId;
					float weight = ai_mesh->mBones[i]->mWeights[j].mWeight;
					bones_id_weight[vertex_id].addBoneData(bone_index, weight);

					// cout << "vertex_id: " << vertex_id << endl;
					// cout << "bone_index: " << bone_index << endl;
					// cout << "weight: " << weight << endl;
				}
			}

			mesh = Mesh(vertices, indices, texture, bones_id_weight, texture_name);
			meshes.push_back(mesh);
		}
	}
};

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

void key_press() {
	// Camera controls
    if (keys_pressed[GLFW_KEY_W])
        camera.ProcessKeyboard(UP);
    
    if (keys_pressed[GLFW_KEY_S])
        camera.ProcessKeyboard(DOWN);
    
    if (keys_pressed[GLFW_KEY_A])
        camera.ProcessKeyboard(LEFT);
    
    if (keys_pressed[GLFW_KEY_D])
        camera.ProcessKeyboard(RIGHT);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS){
            keys_pressed[key] = true;
        }
        if (action == GLFW_RELEASE) {
            keys_pressed[key] = false;
        }
    }

	if ((keys_pressed[GLFW_KEY_SPACE])) {
		if (run_animation)
			run_animation = false;
		else
			run_animation = true;
	}

	if ((keys_pressed[GLFW_KEY_ENTER])) {
		if (blinn_flag) {
			blinn_flag = false;
			cout << "blinn phong: false" << endl;
		}
		else {
			blinn_flag = true;
			cout << "blinn phong: true" << endl;
		}
			
	}
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		mouse_clicked = true;
	} else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		mouse_clicked = false;
		mouse_flag = true;
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}

static unsigned int setup_shader(const char *vertex_shader, const char *fragment_shader)
{
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, (const GLchar**)&vertex_shader, nullptr);

	glCompileShader(vs);	//compile vertex shader

	int status, maxLength;
	char *infoLog = nullptr;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &status);		//get compile status
	if (status == GL_FALSE)								//if compile error
	{
		glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &maxLength);	//get error message length

		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];

		glGetShaderInfoLog(vs, maxLength, &maxLength, infoLog);		//get error message

		fprintf(stderr, "Vertex Shader Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete[] infoLog;
		return 0;
	}
	//	for fragment shader --> same as vertex shader
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, (const GLchar**)&fragment_shader, nullptr);
	glCompileShader(fs);

	glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &maxLength);

		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];

		glGetShaderInfoLog(fs, maxLength, &maxLength, infoLog);

		fprintf(stderr, "Fragment Shader Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete[] infoLog;
		return 0;
	}

	unsigned int program = glCreateProgram();
	// Attach our shaders to our program
	glAttachShader(program, vs);
	glAttachShader(program, fs);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if (status == GL_FALSE)
	{
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);


		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];
		glGetProgramInfoLog(program, maxLength, NULL, infoLog);

		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);

		fprintf(stderr, "Link Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete[] infoLog;
		return 0;
	}
	return program;
}

static std::string readfile(const char *filename)
{
	std::ifstream ifs(filename);
	if (!ifs)
		exit(EXIT_FAILURE);
	return std::string((std::istreambuf_iterator<char>(ifs)),
		(std::istreambuf_iterator<char>()));
}

void render(Model* model)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float time = glfwGetTime();
	float radius = 8.0f;
	glm::vec3 lightPos = glm::vec3(glm::sin(time) * radius, 3.0f, glm::cos(time) * radius);

	glUseProgram(program);
	glUniform3fv(glGetUniformLocation(program, "viewPos"), 1, glm::value_ptr(camera.GetPosition()));
	glUniform3fv(glGetUniformLocation(program, "lightPos"), 1, glm::value_ptr(lightPos));

	glm::mat4 projection = glm::perspective(camera.GetZoom(), 800.0f/600, 1.0f, 100.0f);
	glm::mat4 view = camera.GetViewMatrix();
	glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));

	glm::mat4 eye(1.0f);
	glm::mat4 modelPosition;

	modelPosition = glm::rotate(eye, glm::radians(270.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(eye, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(eye, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	modelPosition = glm::rotate(modelPosition, glm::radians(angle_x * angle_speed), glm::vec3(0.0f, 0.0f, 1.0f));
	modelPosition = glm::rotate(modelPosition, glm::radians(angle_y * angle_speed), glm::vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(modelPosition));
	glUniform1i(glGetUniformLocation(program, "blinnFlag"), (int) blinn_flag);

	model->update();
	model->render(program);

	glUseProgram(light_program);
	glUniformMatrix4fv(glGetUniformLocation(light_program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(light_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
	
	modelPosition = glm::mat4(1.0f);
	modelPosition = glm::translate(modelPosition, lightPos);
	modelPosition = glm::scale(modelPosition, glm::vec3(0.2f));

	glUniformMatrix4fv(glGetUniformLocation(light_program, "model"), 1, GL_FALSE, glm::value_ptr(modelPosition));
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void cubeSetup()
{
	float cube_vertices[] = {
        -0.5f, -0.5f, -0.5f, 
         0.5f, -0.5f, -0.5f, 
         0.5f,  0.5f, -0.5f, 
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, 
        -0.5f,  0.5f,  0.5f, 
        -0.5f, -0.5f,  0.5f, 

        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f
	};

	unsigned int VBO;
	// VAO
	glGenVertexArrays(1, &cubeVAO);
	glBindVertexArray(cubeVAO);

	// Verteices
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

	// Upload postion array
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
	glEnableVertexAttribArray(0);
}

int main(int argc, char *argv[])
{
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	// OpenGL 3.3, Mac OS X is reported to have some problem. However I don't have Mac to test
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);		//set hint to glfwCreateWindow, (target, hintValue)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	//hint--> window not resizable,  explicit use core-profile,  opengl version 3.3
	// For Mac OS X
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow(800, 600, "Animation", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(window);	//set current window as main window to focus
	//glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	// This line MUST put below glfwMakeContextCurrent
	glewExperimental = GL_TRUE;		//tell glew to use more modern technique for managing OpenGL functionality
	glewInit();

	// Enable vsync
	glfwSwapInterval(1);

	// Setup input callback
	glfwSetKeyCallback(window, key_callback);	//set key event handler
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// load shader program
	program = setup_shader(readfile("shader/light.vert").c_str(), readfile("shader/light.frag").c_str());
	light_program = setup_shader(readfile("shader/light_cube.vert").c_str(), readfile("shader/light_cube.frag").c_str());

	Model *model = new Model("model/main_model.dae");
	model->initBoneLoc(program);

	cubeSetup();

	glEnable(GL_DEPTH_TEST);
	// prevent faces rendering to the front while they're behind other faces. 
	glCullFace(GL_BACK);
	// discard back-facing trangle
	// Enable blend mode for billboard
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float last, start;
	last = start = glfwGetTime();
	int fps = 0;

	while (!glfwWindowShouldClose(window))
	{//program will keep draw here until you close the window
		float delta = glfwGetTime() - start;
		double xPos, yPos;

		if (mouse_clicked) {
			glfwGetCursorPos(window, &xPos, &yPos);

			if (mouse_flag) {
				lastX = xPos;
				lastY = yPos;
				mouse_flag = false;
			}
			
			GLfloat xOffset = lastX - xPos;
			GLfloat yOffset = yPos - lastY;

			lastX = xPos;
			lastY = yPos;
			
			//camera.ProcessMouseMovement(xOffset, yOffset);
			angle_x += xOffset;
			angle_y += yOffset;
		}

		key_press();
		
		render(model);

		glfwSwapBuffers(window);	//swap the color buffer and show it as output to the screen.
		glfwPollEvents();		//check if there is any event being triggered
		fps++;
		if (glfwGetTime() - last > 1.0)
		{
			std::cout << (double)fps / (glfwGetTime() - last) << std::endl;
			fps = 0;
			last = glfwGetTime();
		}
	}

	model->releaseObject();
	glDeleteProgram(program);
	glDeleteProgram(light_program);
	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}