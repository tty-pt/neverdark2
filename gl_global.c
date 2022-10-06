#include "gl_global.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "debug.h"

mat4 modelm = {
	{ 1.0f, 0, 0, 0 },
	{ 0, 1.0f, 0, 0 },
	{ 0, 0, 1.0f, 0 },
	{ 0, 0, 0, 1.0f },
};

unsigned modelLoc = 0;
unsigned positionLoc, normalLoc, texCoordLoc;
unsigned white_texture;

unsigned
glg_texture_load(struct texture *tex)
{
	unsigned id;
	CBUG(!tex->data);
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->w, tex->h, 0, GL_RGB, GL_UNSIGNED_BYTE, tex->data);
	stbi_image_free(tex->data);
	glGenerateMipmap(GL_TEXTURE_2D);
	return id;
}

void
glg_init()
{
	struct texture tex;
	tex.data = stbi_load("white.png", &tex.w, &tex.h, &tex.channels, 3);
	white_texture = glg_texture_load(&tex);
}
