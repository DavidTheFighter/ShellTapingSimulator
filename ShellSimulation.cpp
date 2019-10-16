#include "ShellSimulation.h"

#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <util.h>

ShellSimulation::ShellSimulation()
{

}

ShellSimulation::~ShellSimulation()
{

}

/*
This function actually simulates taping. It does it using matrix math mainly, essentially rotating a matrix by each machine axis to find which pixel needs to have a layer added.
*/
void ShellSimulation::simulateTaping(const ShellConfig &shellConfig, const SimulationConfig &simConfig, uint16_t *layermap, uint16_t *scratchLayermap)
{
	uint32_t currentAngleIndex = 0; // The current angle/application
	float rimRotations = 0; // Counter of rim rotations, in radians
	float armRotation = shellConfig.shellArmAngles[currentAngleIndex] * (M_PI / 180.0f); // In radians, 0 = straight up/down
	float shellRotation = 0;

	const float tapeWidthRadians = (shellConfig.tapeWidth / (shellConfig.shellDiameter * M_PI)) * M_PI;
	const float radianFillStepSize = M_2PI / (simConfig.layermapSize * simConfig.mapFillPrecisionMult);

	// Add a circle on the top pole denoting the shell chuck
	const float shellChuckDiameter = shellConfig.shellChuckDiameter;
	float shellChuckContactAngle = M_2PI * (shellChuckDiameter / (M_PI * shellConfig.shellDiameter));

	// Fill a ring around where the shell chuck is, for reference
	for (float f = 0; f < M_2PI; f += radianFillStepSize)
	{
		glm::vec2 uv = convertShellChuckToUV(shellChuckContactAngle, f);

		scratchLayermap[uint32_t(uv.y * float(simConfig.layermapSize - 1)) * simConfig.layermapSize + uint32_t(uv.x * float(simConfig.layermapSize - 1))] = 3;
	}

	// Simulate shell taping
	while (currentAngleIndex < shellConfig.numAngles)
	{
		while (rimRotations < M_2PI * shellConfig.rimRotationsUntilNextAngle[currentAngleIndex])
		{
			// Fill in the layermap where the tape is
			for (float t = -tapeWidthRadians / 2.0f; t <= tapeWidthRadians / 2.0f; t += radianFillStepSize)
			{
				glm::vec2 uv = convertShellToUV(rimRotations, armRotation, t, shellRotation);

				scratchLayermap[uint32_t(uv.y * float(simConfig.layermapSize - 1)) * simConfig.layermapSize + uint32_t(uv.x * float(simConfig.layermapSize - 1))] = 1;
			}

			// Once every full rotation, reset the scratchmap
			if (std::fmod(rimRotations, M_2PI) > std::fmod(rimRotations + radianFillStepSize, M_2PI))
			{
				for (uint32_t i = 0; i < simConfig.layermapSize * simConfig.layermapSize; i++)
					layermap[i] += scratchLayermap[i];

				memset(scratchLayermap, 0, sizeof(scratchLayermap[0]) * simConfig.layermapSize * simConfig.layermapSize);
			}

			// Step all the machine axes
			shellRotation += radianFillStepSize * shellConfig.shellStepperSpeed[currentAngleIndex];
			rimRotations += radianFillStepSize;
		}

		// Setup for the next angle/application
		currentAngleIndex++;
		rimRotations = std::fmod(rimRotations, radianFillStepSize);

		armRotation = shellConfig.shellArmAngles[currentAngleIndex] * (M_PI / 180.0f);
	}
}

uint32_t ShellSimulation::computeLayermapError(const SimulationConfig &simConfig, uint32_t targetLayers, uint16_t *layermap)
{
	float totalError = 0.0f;

	// Calculate error based on delta from the target layers
	float absErr = 0;
	float errFactor = 1.0f / float(simConfig.layermapSize);

	for (uint32_t x = 0; x < simConfig.layermapSize; x += simConfig.layermapSize / simConfig.errorCalcYAxisSweeps)
	{
		for (uint32_t y = 0; y < simConfig.layermapSize; y++)
		{
			uint32_t aboveLayerCount = layermap[std::max(int32_t(y) - 1, 0) * simConfig.layermapSize + x];
			uint32_t layerCount = layermap[y * simConfig.layermapSize + x];
			float layerError = std::pow(std::abs(float(targetLayers) - float(layerCount)), 4.0f);
			float smoothnessError = std::pow(std::abs(float(layerCount) - float(aboveLayerCount)), 4.0f);

			absErr += (layerError + smoothnessError) * errFactor;
		}
	}

	return uint32_t(absErr / float(simConfig.errorCalcYAxisSweeps));
}
