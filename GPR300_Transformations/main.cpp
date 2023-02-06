#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "EW/Shader.h"
#include "EW/ShapeGen.h"

void resizeFrameBufferCallback(GLFWwindow* window, int width, int height);
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);

float lastFrameTime;
float deltaTime;

int SCREEN_WIDTH = 1080;
int SCREEN_HEIGHT = 720;
float nP = 0.1;
float fP = 50;

double prevMouseX;
double prevMouseY;
bool firstMouseInput = false;

/* Button to lock / unlock mouse
* 1 = right, 2 = middle
* Mouse will start locked. Unlock it to use UI
* */
const int MOUSE_TOGGLE_BUTTON = 1;
const float MOUSE_SENSITIVITY = 0.1f;

glm::vec3 bgColor = glm::vec3(0);
float oRadius = 4.0f;
float oSpeed = 1.0f;
float fView = 8.0f;
float oHeight = 5.0f;
bool oToggle = false;

namespace ew {
	glm::mat4 scale(glm::vec3 s) {
		return glm::mat4(
			s.x, 0, 0, 0,
			0, s.y, 0, 0,
			0, 0, s.z, 0,
			0, 0, 0, 1
		);
	}

	glm::mat4 rotateX(glm::vec3 r) {
		return glm::mat4(
			1, 0, 0, 0,
			0, (cos(r.x)), (sin(r.x)), 0,
			0, (-1 * sin(r.x)), (cos(r.x)), 0,
			0, 0, 0, 1
		);
	}

	glm::mat4 rotateY(glm::vec3 r) {
		return glm::mat4(
			cos(r.y), 0, (-1 * sin(r.y)), 0,
			0, 1, 0, 0,
			sin(r.y), 0, cos(r.y), 0,
			0, 0, 0, 1
		);
	}

	glm::mat4 rotateZ(glm::vec3 r) {
		return glm::mat4(
			cos(r.z), sin(r.z), 0, 0,
			(-1 * sin(r.z)), cos(r.z), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);
	}

	glm::mat4 translate(glm::vec3 p) {
		return glm::mat4(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			p.x, p.y, p.z, 1
		);
	}
}

struct Transform {
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 scale = glm::vec3(1);


	glm::mat4 getModelMatrix() {
		glm::mat4 modelMatrix = glm::mat4(1);

		modelMatrix *= ew::translate(position);
		modelMatrix *= ew::rotateX(rotation);
		modelMatrix *= ew::rotateY(rotation);
		modelMatrix *= ew::rotateZ(rotation);
		modelMatrix *= ew::scale(scale);

		return modelMatrix;
	}
};

struct Camera {
	glm::vec3 position = glm::vec3(0, 0, 5);
	glm::vec3 target = glm::vec3(0);
	float fov = 0.0;
	float orthographicSize = 0.0;
	bool orthographic = false;

	glm::mat4 getViewMatrix() {
		glm::vec3 up = glm::vec3(0, 1, 0);

		glm::vec3 oForward = glm::normalize((position - target));
		glm::vec3 oRight = glm::normalize(cross(up, oForward));
		glm::vec3 oUp = cross(oForward, oRight);

		//oForward *= -1;

		glm::mat4 rCam = {
			oRight.x, oUp.x, oForward.x, 0,
			oRight.y, oUp.y, oForward.y, 0,
			oRight.z, oUp.z, oForward.z, 0,
			0, 0, 0, 1
		};

		glm::mat4 tCam = {
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			(position.x * -1), (position.y * -1), (position.z * -1), 1
		};

		glm::mat4 vmx = rCam * tCam;

		return vmx;
	}

	glm::mat4 getProjectionMatrix() {
		glm::mat4 pjx = glm::mat4(1);

		if (orthographic) {
			pjx = ortho();
		}
		else {
			pjx = perspective();
		}

		return pjx;
	}

	glm::mat4 ortho() {
		float w = orthographicSize * (SCREEN_HEIGHT / SCREEN_WIDTH);
		float r = w / 2;
		float t = orthographicSize / 2;
		float l = -r;
		float b = -t;

		glm::mat4 ort = {
			(2 / (r - l)), 0, 0, 0,
			0, (2 / (t - b)), 0, 0,
			0, 0, ((2 / (fP - nP)) * -1), 0,
			((r + l) / (r - l) * -1), ((t + b) / (t - b) * -1), ((fP + nP) / (fP - nP) * -1), 1
		};

		return ort;
	}

	glm::mat4 perspective() {
		float c = glm::tan(glm::radians(fov) / 2);
		float aR = (SCREEN_HEIGHT / SCREEN_WIDTH);

		glm::mat4 psp = {
			(1 / (aR * c)), 0, 0, 0,
			0, (1 / c), 0, 0,
			0, 0, ((fP + nP) / (fP - nP) * -1), -1,
			0, 0, ((2 * fP * nP) / (fP - nP) * -1), 1
		};

		return psp;
	}
};

const int NUM_CUBES = 8;
Transform transforms[NUM_CUBES];
Camera cam;

int main() {
	if (!glfwInit()) {
		printf("glfw failed to init");
		return 1;
	}

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Transformations", 0, 0);
	glfwMakeContextCurrent(window);

	if (glewInit() != GLEW_OK) {
		printf("glew failed to init");
		return 1;
	}

	glfwSetFramebufferSizeCallback(window, resizeFrameBufferCallback);
	glfwSetKeyCallback(window, keyboardCallback);

	// Setup UI Platform/Renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	//Dark UI theme.
	ImGui::StyleColorsDark();

	Shader shader("shaders/vertexShader.vert", "shaders/fragmentShader.frag");

	MeshData cubeMeshData;
	createCube(1.0f, 1.0f, 1.0f, cubeMeshData);

	Mesh cubeMesh(&cubeMeshData);

	//Enable back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// delcare a random seed
	srand(time(NULL));

	// create a random scale, rotation, and translation for each cube
	for (size_t i = 0; i < NUM_CUBES; i++)
	{
		float scale = rand() % 5 + 1;

		float rotX = rand() % 10 + 1;
		float rotY = rand() % 10 + 1;
		float rotZ = rand() % 10 + 1;

		float transX = rand() % 5 + 1;
		float transY = rand() % 5 + 1;
		float transZ = rand() % 5 + 1;

		transforms[i].position = glm::vec3(transX, transY, transZ);
		transforms[i].rotation = glm::vec3(rotX, rotY, rotZ);
		transforms[i].scale = glm::vec3(scale);
	}

	while (!glfwWindowShouldClose(window)) {
		glClearColor(bgColor.r,bgColor.g,bgColor.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float time = (float)glfwGetTime();
		deltaTime = time - lastFrameTime;
		lastFrameTime = time;

		//Draw
		for (size_t i = 0; i < NUM_CUBES; i++)
		{
			cam.position.x = oRadius * (glm::sin(oSpeed * time));
			cam.position.z = oRadius * (glm::cos(oSpeed * time));
			cam.fov = fView;

			cam.orthographicSize = oHeight;
			cam.orthographic = oToggle;

			shader.use();
			shader.setMat4("_Projection", cam.getProjectionMatrix());
			shader.setMat4("_View", cam.getViewMatrix());
			shader.setMat4("_Model", transforms[i].getModelMatrix());

			cubeMesh.draw();
		}

		//Draw UI
		ImGui::Begin("Settings");
		ImGui::SliderFloat("Orbit Radius", &oRadius, 0.0f, 10.0f);
		ImGui::SliderFloat("Orbit Speed", &oSpeed, 0.0f, 10.0f);
		ImGui::SliderFloat("Field of View", &fView, 0.0f, 10.0f);
		ImGui::SliderFloat("Orthographic Height", &oHeight, 0.0f, 10.0f);
		ImGui::Checkbox("Orthographic Toggle", &oToggle);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwPollEvents();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

void resizeFrameBufferCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	SCREEN_WIDTH = width;
	SCREEN_HEIGHT = height;
}

void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
	if (keycode == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
}
