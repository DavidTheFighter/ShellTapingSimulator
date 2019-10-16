#pragma once

#include <cstdint>
#include <vector>

struct SimulationConfig
{
	uint32_t layermapSize; // Width & height of the map used to simulate layers
	float mapFillPrecisionMult; // Multiplier for the precision when filling the layermap (a good starting value is 2-3)
	uint32_t errorCalcYAxisSweeps; // How many Y axis sweeps to do across a layermap image when calculating the error, error is averaged for all the sweeps (a good starting value is 8)
};

struct SearchConfig
{
	uint32_t numAngles; // Number of angles/applications per taping session
	float shellDiameter; // Outside diameter of the shell hemisphere in inches
	float tapeWidth; // Width of the tape used in inches
	float shellChuckDiameter; // The diameter of the chuck (what holds the shell in place) in diameter

	uint32_t targetLayers; // The number of layers intended to be put on the shell
	uint32_t maxIterations; // The maximum number of iterations or searches to do
	
	// The min and max angles per application that will be searched
	std::vector<float> minShellArmAngles;
	std::vector<float> maxShellArmAngles;

	// The min and max stepper speeds per angle/application
	std::vector<float> minShellStepperSpeed;
	std::vector<float> maxShellStepperSpeed;

	// There are no min/max rimRotationsUntilNextAngle as only one shell rotation per angle/application is considered
};

struct ShellConfig
{
	uint32_t numAngles; // Number of angles/applications per taping session
	float shellDiameter; // Outside diameter of the shell hemisphere in inches
	float tapeWidth; // Width of the tape used in inches
	float shellChuckDiameter; // The diameter of the chuck (what holds the shell in place) in diameter

	std::vector<float> shellArmAngles; // Each application's angle, in degrees
	std::vector<float> shellStepperSpeed; // Each angle's speed for the shell stepper motor, in fractions relative to the rim speed
	std::vector<float> rimRotationsUntilNextAngle; // The number of rim rotations until the next angle, typically 1 / shellStepperSpeed to allow a full shell rotation per angle
};

class ShellSimulation
{
public:
	ShellSimulation();
	virtual ~ShellSimulation();

	/*
	Simulates a taping session on a shell. 
	@param[out] layermap The image to write the layers to, all elements must be set to 0
	@param[out] scratchLayermap An image the same size as "layermap" used for calculations, all elements must be set to 0
	*/
	void simulateTaping(const ShellConfig &shellConfig, const SimulationConfig &simConfig, uint16_t *layermap, uint16_t *scratchLayermap);

	/*
	Computes the average error of a computed shell layermap.

	@return A positive integer with no bounds besides uint32_t limits.
	*/
	uint32_t computeLayermapError(const SimulationConfig &simConfig, uint32_t targetLayers, uint16_t *layermap);
};

