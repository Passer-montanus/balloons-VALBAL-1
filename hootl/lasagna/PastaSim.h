#ifndef PASTASIM_H
#define PASTASIM_H
#include <random>
#include <ctime>
#include "Utils.h"

/**
 * This is the simulator class for altitude feedback controllers run on vabal. 
 * It contians the constants and state variables necessary to simulate the balloon 
 * dynamics in the atmosphere
 */
class PastaSim
{
public:
	// typedef struct {
	// 	bool nightfall = 1;						// boolean to toggle nightfall on or off
	// 	float k_drag = 0.006;                   // Linearized drag constant of the balloon, in m/s per g
	// 	float val_dldt = 3;						// change in lift per unit time cuased by venting, in g/s 
	// 	float bal_dldt = 0.8;					// change in lift per unit time cuased by ballast, in g/s 
	// 	float freq = 20;                        // frequency of the simulation, or in other words, 1 over sim timestep
	// 	unsigned int sun_calc_interval = 60000; // interval for how often sun posisition is calculated, in ms
	// 	float lon = -122.1697;                  // longitude of balloon (used for nightfall)
	// 	float lat = 37.4275;                    // latitude of balloon (used for nightfall)
	// 	SunsetPredictor::GPSTime gtime;
	// } Config;

	struct Config{
		bool nightfall = 1;						// boolean to toggle nightfall on or off
		float k_drag = 0.006;                   // Linearized drag constant of the balloon, in m/s per g
		float val_dldt = 3;						// change in lift per unit time cuased by venting, in g/s 
		float bal_dldt = 0.8;					// change in lift per unit time cuased by ballast, in g/s 
		float freq = 20;                        // frequency of the simulation, or in other words, 1 over sim timestep
		unsigned int sun_calc_interval = 60000; // interval for how often sun posisition is calculated, in ms
		float lon = -122.1697;                  // longitude of balloon (used for nightfall)
		float lat = 37.4275;                    // latitude of balloon (used for nightfall)
		SunsetPredictor::GPSTime gtime;
	};

	Config conf;
	PastaSim(int seed);
	PastaSim();
	float evolve(float action);
	float sunset_dldt = 0;
	SunsetPredictor sunpred;
	unsigned int unix_offset;
	unsigned long time = 0;				// sim time in ms
	float h;							// sim altitude in m
	float v;							// sim velocity in m/s
	float l;							// sim balloon lift in g 
	unsigned int sun_pred_ctr = 0;
private:
	void init();
	std::default_random_engine gen;
	std::normal_distribution<float> l_noise;
};


#endif