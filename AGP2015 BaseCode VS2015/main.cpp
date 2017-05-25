// Completed shadow mapping tutorial by B00273607 - based on tutorial available on http://learnopengl.com/#!Advanced-Lighting/Shadows/Shadow-Mapping
// Controls: WASD movement, R and F to move vertically
// Light can be moved with arrow keys and up and down with O and P
// IMPORTANT NOTE: drastic light movement might cause the implementation to break because of limited size of shadowmap texture
// Also: you can comment out the MD2 animation if your machine struggles with it.


// Windows specific: Uncomment the following line to open a console window for debug output
#if _DEBUG
#pragma comment(linker, "/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")
#endif

#include "rt3d.h"
#include "rt3dObjLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stack>
#include "md2model.h"


using namespace std;

#define DEG_TO_RADIAN 0.017453293

// Globals
// Real programs don't use globals :-D

GLuint meshIndexCount = 0;
GLuint toonIndexCount = 0;
GLuint md2VertCount = 0;
GLuint meshObjects[3];

GLuint textureProgram;
GLuint shadowShaderProgram;
GLuint depthShaderProgram;

GLfloat r = 0.0f;

glm::vec3 eye(-2.0f, 1.0f, 8.0f);
glm::vec3 at(0.0f, 1.0f, -1.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);

stack<glm::mat4> mvStack; 

//////////////////
/// FBO globals
//////////////////
GLuint depthMapFBO; // FBO
GLuint depthMap;	// FBO texture
const GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
const GLuint screenWidth = 800, screenHeight = 600;

// TEXTURE STUFF
GLuint textures[4];
GLuint skybox[5];

rt3d::lightStruct light0 = {
	{0.4f, 0.4f, 0.4f, 1.0f}, // ambient
	{1.0f, 1.0f, 1.0f, 1.0f}, // diffuse
	{1.0f, 1.0f, 1.0f, 1.0f}, // specular
	{-5.0f, 2.0f, 2.0f, 1.0f}  // position
};
glm::vec4 lightPos(-6.0f, 5.0f, 4.0f, 1.0f); //light position
glm::vec3 lightPos3(0.0f, 0.0f, 0.0f);

// Movement and control variables
float theta = 0.0f;
float moveVar = 0.0f;
bool switchMov = false;
bool toggleDepthMap = false;

// md2 stuff
md2model tmpModel;
int currentAnim = 0;

// Set up rendering context
SDL_Window * setupRC(SDL_GLContext &context) {
	SDL_Window * window;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) // Initialize video
        rt3d::exitFatalError("Unable to initialize SDL"); 
	  
    // Request an OpenGL 3.0 context.
	
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); 

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);  // double buffering on
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8); // 8 bit alpha buffering
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // Turn on x4 multisampling anti-aliasing (MSAA)
 
    // Create 800x600 window
    window = SDL_CreateWindow("SDL/GLM/OpenGL Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN );
	if (!window) // Check window was created OK
        rt3d::exitFatalError("Unable to create window");
 
    context = SDL_GL_CreateContext(window); // Create opengl context and attach to window
    SDL_GL_SetSwapInterval(1); // set swap buffers to sync with monitor's vertical refresh rate
	return window;
}

// A simple texture loading function
// lots of room for improvement - and better error checking!
GLuint loadBitmap(char *fname) {
	GLuint texID;
	glGenTextures(1, &texID); // generate texture ID

	// load file - using core SDL library
 	SDL_Surface *tmpSurface;
	tmpSurface = SDL_LoadBMP(fname);
	if (!tmpSurface) {
		std::cout << "Error loading bitmap" << std::endl;
	}

	// bind texture and set parameters
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SDL_PixelFormat *format = tmpSurface->format;
	
	GLuint externalFormat, internalFormat;
	if (format->Amask) {
		internalFormat = GL_RGBA;
		externalFormat = (format->Rmask < format-> Bmask) ? GL_RGBA : GL_BGRA;
	}
	else {
		internalFormat = GL_RGB;
		externalFormat = (format->Rmask < format-> Bmask) ? GL_RGB : GL_BGR;
	}

	glTexImage2D(GL_TEXTURE_2D,0,internalFormat,tmpSurface->w, tmpSurface->h, 0,
		externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);

	SDL_FreeSurface(tmpSurface); // texture loaded, free the temporary buffer
	return texID;	// return value of texture ID
}

// Function that initializes shaders, objects and so on
void init(void) {
	// Setting up the shaders
	shadowShaderProgram = rt3d::initShaders("shadows.vert", "shadows.frag");
	textureProgram = rt3d::initShaders("textured.vert", "textured.frag");
	depthShaderProgram = rt3d::initShaders("simpleDepth.vert", "simpleDepth.frag");

	vector<GLfloat> verts;
	vector<GLfloat> norms;
	vector<GLfloat> tex_coords;
	
	vector<GLuint> indices;
	rt3d::loadObj("cube.obj", verts, norms, tex_coords, indices);
	meshIndexCount = indices.size();
	textures[0] = loadBitmap("fabric.bmp");
	meshObjects[0] = rt3d::createMesh(verts.size()/3, verts.data(), nullptr, norms.data(), tex_coords.data(), meshIndexCount, indices.data());
	
	textures[1] = loadBitmap("hobgoblin2.bmp");
	meshObjects[1] = tmpModel.ReadMD2Model("tris.MD2");
	md2VertCount = tmpModel.getVertDataCount();
	
	textures[2] = loadBitmap("studdedmetal.bmp");
	textures[3] = loadBitmap("tex3.bmp");
		
	verts.clear(); norms.clear();tex_coords.clear();indices.clear();
	rt3d::loadObj("bunny-5000.obj", verts, norms, tex_coords, indices);
	toonIndexCount = indices.size();
	meshObjects[2] = rt3d::createMesh(verts.size()/3, verts.data(), nullptr, norms.data(), nullptr, toonIndexCount, indices.data());

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	////////////////////
	/// FBO for shadows
	/////////////////////

	glGenFramebuffers(1, &depthMapFBO);
	glGenTextures(1, &depthMap);

	// Bind FBO, RBO & Texture & init storage and params
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthMapFBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1024, 1024);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	// Need to set mag and min params
	// otherwise mipmaps are assumed
	// This fixes problems with NVIDIA cards
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// Check for errors
	GLenum valid = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if (valid != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer Object not complete" << std::endl;
	if (valid == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
		std::cout << "Framebuffer incomplete attachment" << std::endl;
	if (valid == GL_FRAMEBUFFER_UNSUPPORTED)
		std::cout << "FBO attachments unsupported" << std::endl;
		
}

// Functions used for camera movement
glm::vec3 moveForward(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d*std::sin(r*DEG_TO_RADIAN), pos.y, pos.z - d*std::cos(r*DEG_TO_RADIAN));
}
glm::vec3 moveRight(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d*std::cos(r*DEG_TO_RADIAN), pos.y, pos.z + d*std::sin(r*DEG_TO_RADIAN));
}

// mainly used for controls; also updates a downscaled vec3 light position vector
void update(void) {
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	if ( keys[SDL_SCANCODE_W] ) eye = moveForward(eye,r,0.1f);
	if ( keys[SDL_SCANCODE_S] ) eye = moveForward(eye,r,-0.1f);
	if ( keys[SDL_SCANCODE_A] ) eye = moveRight(eye,r,-0.1f);
	if ( keys[SDL_SCANCODE_D] ) eye = moveRight(eye,r,0.1f);
	if ( keys[SDL_SCANCODE_R] ) eye.y += 0.1;
	if ( keys[SDL_SCANCODE_F] ) eye.y -= 0.1;

	if ( keys[SDL_SCANCODE_UP] ) lightPos[2] -= 0.1;
	if ( keys[SDL_SCANCODE_LEFT] ) lightPos[0] -= 0.1;
	if ( keys[SDL_SCANCODE_DOWN] ) lightPos[2] += 0.1;
	if ( keys[SDL_SCANCODE_RIGHT] ) lightPos[0] += 0.1;
	if (keys[SDL_SCANCODE_O]) lightPos[1] += 0.1;
	if (keys[SDL_SCANCODE_P]) lightPos[1] -= 0.1;

	if ( keys[SDL_SCANCODE_COMMA] ) r -= 1.0f;
	if ( keys[SDL_SCANCODE_PERIOD] ) r += 1.0f;


	for (int i = 0;i < 3;i++)
		lightPos3[i] = lightPos[i];

	if ( keys[SDL_SCANCODE_1] ) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);
	}
	if ( keys[SDL_SCANCODE_2] ) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);
	}

	if (keys[SDL_SCANCODE_J]) toggleDepthMap = true;
	if (keys[SDL_SCANCODE_K]) toggleDepthMap = false;
}

// Rendering functions; each of these renders a different part of the scene
// For the sake of simplicity and not causing confusion, we reset the model matrix instead of pushing an identity to the modelview stack
void renderBaseCube(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-10.0f, -0.1f, -10.0f));
	model = glm::scale(model, glm::vec3(20.0f, 0.1f, 20.0f));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
}

void renderSpinningCube(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-6.0f, 1.0f, -3.0f));
	model = glm::rotate(model, float(theta*DEG_TO_RADIAN), glm::vec3(1.0f, 1.0f, 1.0f));
	model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
}

void renderTallCubes(GLuint shader) {
	glm::mat4 model;
	for (int b = 0; b <5; b++) {
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(-10.0f + b * 2, 1.0f, -12.0f + b * 2));
		model = glm::scale(model, glm::vec3(0.5f, 1.0f + b, 0.5f));
		rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
		rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
	}
}

void renderBunny(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-7.0f, 0.5f, -2.0f));
	model = glm::scale(model, glm::vec3(10.0, 10.0, 10.0));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[2], toonIndexCount, GL_TRIANGLES);
}

void renderHobgoblin(GLuint shader) {
	// animation can adversely impact performace on some machine; comment out these two lines in that case.
	// In a real case scenario it would be sensible to animate it outside the rendering function, as calling this twice will make all animations twice as fast
	tmpModel.Animate(currentAnim, 0.1);
	rt3d::updateMesh(meshObjects[1], RT3D_VERTEX, tmpModel.getAnimVerts(), tmpModel.getVertDataSize());

	// draw the hobgoblin
	glCullFace(GL_FRONT); // md2 faces are defined clockwise, so cull front face
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	//rt3d::materialStruct tmpMaterial = material1;
	//rt3d::setMaterial(shaderProgram, material1);
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-8.0f, 1.2f, -6.0f));
	model = glm::rotate(model, float(90.0f*DEG_TO_RADIAN), glm::vec3(-1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(1.0*0.05, 1.0*0.05, 1.0*0.05));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawMesh(meshObjects[1], md2VertCount, GL_TRIANGLES);
	glCullFace(GL_BACK);

	// reset texture
	glBindTexture(GL_TEXTURE_2D, 0);
}

void renderMovingCube(GLuint shader) {
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(-7.0f+moveVar, 4.0f, -2.0f+moveVar));
	model = glm::rotate(model, float(theta*DEG_TO_RADIAN), glm::vec3(1.0f, 1.0f, 1.0f));
	model = glm::scale(model, glm::vec3(0.3f, 0.5f, 0.6f));
	rt3d::setUniformMatrix4fv(shader, "model", glm::value_ptr(model));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
}

// updates variables to move objects in the scene (for testing purposes)
void moveObjects() {
	theta += 2.0f;
	if (moveVar >= 3.0f)
		switchMov = false;
	if (moveVar <= -3.0f)
		switchMov = true;
	if (switchMov == true) {
		moveVar += 0.02f;
	}
	if (switchMov == false) {
		moveVar -= 0.02f;
	}
}

// main render function, sets up the shaders and then calls all other functions
void RenderShadowScene(glm::mat4 projection, glm::mat4 lightSpaceMatrix, glm::mat4 viewMatrix, GLuint shader) {

	
	glUseProgram(shader);
	rt3d::setUniformMatrix4fv(shader, "lightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));
	rt3d::setUniformMatrix4fv(shader, "projection", glm::value_ptr(projection));
	rt3d::setUniformMatrix4fv(shader, "view", glm::value_ptr(viewMatrix));
	GLuint uniformIndex = glGetUniformLocation(shader, "lightPos");
	glUniform3fv(uniformIndex, 1, glm::value_ptr(lightPos3));
	uniformIndex = glGetUniformLocation(shader, "viewPos");
	glUniform3fv(uniformIndex, 1, glm::value_ptr(eye));

	uniformIndex = glGetUniformLocation(shader, "diffuseTexture");
	glUniform1i(uniformIndex, 1);
	uniformIndex = glGetUniformLocation(shader, "shadowMap");
	glUniform1i(uniformIndex, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures[3]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	
		
	renderBaseCube(shader);
	renderSpinningCube(shader);
	renderTallCubes(shader);
	renderMovingCube(shader);
	renderBunny(shader);
	renderHobgoblin(shader);

}

// draw function called in the main loop
void draw(SDL_Window * window) {

	// clear the screen
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection(1.0);
	projection = glm::perspective(float(60.0f*DEG_TO_RADIAN), 800.0f / 600.0f, 1.0f, 150.0f);
	GLfloat scale(1.0f); // just to allow easy scaling of complete scene

	glm::mat4 modelview(1.0); // set base position for scene
	mvStack.push(modelview);

	glm::mat4 lightProjection, lightView;
	GLfloat near_plane = -10.0f, far_plane = 20.0f;
	// consider making bigger
	lightProjection = glm::ortho<float>(-20, 0, -10, 10, near_plane, far_plane);
	lightView = glm::lookAt(lightPos3, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	
	glm::mat4 lightSpaceMatrix;
	lightSpaceMatrix = lightProjection * lightView;


	// render in two passes
	// first shadow to FBO
	// then scene using depthmap data
	moveObjects();

	for (int pass = 0; pass < 2; pass++) {


		// 'eye' is the world position of the camera
		at = moveForward(eye, r, 1.0f);
		mvStack.top() = glm::lookAt(eye, at, up);

		if (pass == 0) {
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
			glDrawBuffer(GL_NONE); // no need for colour buffer (optional)
			glReadBuffer(GL_NONE); // (optional)
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear FBO
			glClear(GL_DEPTH_BUFFER_BIT);
			
			glCullFace(GL_FRONT); // cull front faces when drawing to shadow map, to fix some issues
			RenderShadowScene(lightProjection, lightSpaceMatrix, lightView, depthShaderProgram); // render using light's point of view and simpler shader program
			glCullFace(GL_BACK); // don't forget to reset original culling face


		}
		else {

			//Render to frame buffer
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glViewport(0, 0, screenWidth, screenHeight);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear window
																// clear the screen
			glEnable(GL_CULL_FACE);
			glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
			RenderShadowScene(projection, lightSpaceMatrix,mvStack.top(), shadowShaderProgram); // render normal scene from normal point of view
		}

		if (pass == 1 && toggleDepthMap == true) { // on second pass, if depthmap quad is toggled
						
			///////////////////
			// Draw depthmap
			///////////////////
			// reset to identity matrix to draw as HUD, as well as disabling depth test
			projection = glm::mat4(1.0);
			glDisable(GL_DEPTH_TEST);
			glUseProgram(textureProgram);
			rt3d::setUniformMatrix4fv(textureProgram, "projection",
				glm::value_ptr(projection));

			// Reset the model view matrix and draw a textured quad
			// push identity matrix to draw as HUD
			mvStack.push(glm::mat4(1.0));
			mvStack.top() = glm::scale(mvStack.top(), glm::vec3(1.0f, -1.0f, 1.0f));
			rt3d::setUniformMatrix4fv(textureProgram, "modelview", glm::value_ptr(mvStack.top()));

			// set tex sampler to texture unit 0, arbitrarily
			GLuint uniformIndex = glGetUniformLocation(textureProgram, "textureUnit0");
			glUniform1i(uniformIndex, 0);
			// Now bind textures to texture unit0 & draw the quad
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthMap);
			glCullFace(GL_BACK);
			rt3d::drawIndexedMesh(meshObjects[0], 6, GL_TRIANGLES); // Draw a quad using cube data
			glBindTexture(GL_TEXTURE_2D, 0);	// unbind the texture
			mvStack.pop();
			glEnable(GL_DEPTH_TEST);	
		}

		glDepthMask(GL_TRUE);
	}

	SDL_GL_SwapWindow(window); // swap buffers

}

// Program entry point - SDL manages the actual WinMain entry point for us
int main(int argc, char *argv[]) {
    SDL_Window * hWindow; // window handle
    SDL_GLContext glContext; // OpenGL context handle
    hWindow = setupRC(glContext); // Create window and render context 

	// Required on Windows *only* init GLEW to access OpenGL beyond 1.1
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err) { // glewInit failed, something is seriously wrong
		std::cout << "glewInit failed, aborting." << endl;
		exit (1);
	}
	cout << glGetString(GL_VERSION) << endl;

	init();

	bool running = true; // set running to true
	SDL_Event sdlEvent;  // variable to detect SDL events
	while (running)	{	// the event loop
		while (SDL_PollEvent(&sdlEvent)) {
			if (sdlEvent.type == SDL_QUIT)
				running = false;
		}
		update();
		draw(hWindow); // call the draw function
	}

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(hWindow);
    SDL_Quit();
    return 0;
}