#ifndef ACT_BANDSTRUCTURE_H
#define ACT_BANDSTRUCTURE_H

#include "points.h"
#include "bandstructure.h"
#include "state.h"
#include "window.h"
#include "statistics.h"
#include "harmonic.h"

class ActiveBandStructure {
private:
	// note: we use std::vector because we want a variable number of bands
	// per each k-point
	std::vector<double> energies;
	std::vector<double> groupVelocities;
	std::vector<std::complex<double>> velocities;
	std::vector<std::complex<double>> eigenvectors;
	ActivePoints * activePoints = nullptr;
	Statistics statistics;
	bool hasEigenvectors = false;
	long numStates = 0;
	long numAtoms = 0;
	VectorXl numBands;

	// index management
	// these are two auxiliary vectors to store indices
	MatrixXl auxBloch2Comb;
	VectorXl cumulativeKbOffset;
	VectorXl cumulativeKbbOffset;
	// this is the functionality to build the indices
	void buildIndeces(); // to be called after building the band structure
	// and these are the tools to convert indices

	// utilities to convert Bloch indices into internal indices
	long velBloch2Comb(long & ik, long & ib1, long & ib2, long & i);
	long gvelBloch2Comb(long & ik, long & ib, long & i);
	long eigBloch2Comb(long & ik, long & i, long & iat, long & ib);
	long bloch2Comb(long & k, long & b);

	long numPoints;
	bool hasPoints();
public:
	ActiveBandStructure(Statistics & statistics_);
	Statistics getStatistics();
	long getNumPoints();
	Point getPoint(const long & pointIndex);
	long getNumStates();
	State getState(Point & point);  // returns all bands at fixed k/q-point

	double getEnergy(long & stateIndex);
	Eigen::Vector3d getGroupVelocity(long & stateIndex);

	std::tuple<long,long> comb2Bloch(long & is);

	ActivePoints buildOnTheFly(Window & window, FullPoints & fullPoints,
			HarmonicHamiltonian & h0);

	ActivePoints buildAsPostprocessing(Window & window,
			FullBandStructure<FullPoints> & fullBandStructure);
};

#endif
