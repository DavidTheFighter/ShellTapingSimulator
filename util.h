#pragma once

#define M_PI 3.1415926535897932384
#define M_2PI 6.2831853071795864769

inline float saturate(float f)
{
	return f > 1.0f ? 1.0f : (f < 0.0f ? 0.0f : f);
}

// Converts a unit vector direction to UV coordinates
inline glm::vec2 convertDirToUV(glm::vec3 dir)
{
	glm::vec2 uv = glm::vec2(std::atan2(dir.x, dir.z), std::acos(dir.y));

	uv.x = saturate((uv.x + M_PI) / M_2PI);
	uv.y = saturate((uv.y) / M_PI);

	return glm::vec2(uv.x, uv.y);
}

/*
// Converts UV - [0, 1]
inline glm::vec3 convertUVToDir(glm::vec2 uv)
{
	return glm::vec3(sin(uv.y) * cos(uv.x), sin(uv.y) * sin(uv.x), cos(uv.y));
}
*/

inline glm::vec3 convertRimToDir(float rimRotation)
{
	return glm::vec3(sin(rimRotation) * cos(0), sin(rimRotation) * sin(0), cos(rimRotation));
}

inline glm::vec2 convertRimToUV(float rimRotation)
{
	return convertDirToUV(convertRimToDir(rimRotation));
}

inline glm::vec2 convertShellToUV(float rimRotation, float armRotation, float tapeRotation, float shellRotation)
{
	glm::mat4 rotation = glm::mat4(1);
	rotation = glm::rotate(rotation, shellRotation, glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, armRotation, glm::vec3(0, 0, 1));
	rotation = glm::rotate(rotation, rimRotation, glm::vec3(1, 0, 0));
	rotation = glm::rotate(rotation, tapeRotation, glm::vec3(0, 0, 1));

	glm::vec4 dir = rotation * glm::vec4(0, 1, 0, 1);

	return convertDirToUV(glm::vec3(dir.x, dir.y, dir.z));
}

inline glm::vec2 convertShellChuckToUV(float chuckRotation, float step)
{
	glm::mat4 rotation = glm::mat4(1);
	rotation = glm::rotate(rotation, step, glm::vec3(0, 1, 0));
	rotation = glm::rotate(rotation, chuckRotation, glm::vec3(1, 0, 0));

	glm::vec4 dir = rotation * glm::vec4(0, 1, 0, 1);

	return convertDirToUV(glm::vec3(dir.x, dir.y, dir.z));
}