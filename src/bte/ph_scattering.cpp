#include "ph_scattering.h"
#include "constants.h"

PhScatteringMatrix::PhScatteringMatrix(Context & context_,
			StatisticsSweep & statisticsSweep_,
			DeltaFunction * smearing_,
			FullBandStructure<FullPoints> & innerBandStructure_,
			FullBandStructure<FullPoints> & outerBandStructure_,
			Interaction3Ph * coupling3Ph_) :
			ScatteringMatrix(context_, statisticsSweep_, smearing_,
					innerBandStructure_, outerBandStructure_),
			coupling3Ph(coupling3Ph_) {
//	couplingIsotope = couplingIsotope_;
//	couplingBoundary = couplingBoundary_;
}

PhScatteringMatrix::PhScatteringMatrix(const PhScatteringMatrix & that) :
		ScatteringMatrix(that), coupling3Ph(that.coupling3Ph) {
//		couplingIsotope(that.couplingIsotope);
//		couplingBoundary(that.couplingBoundary);
}

PhScatteringMatrix & PhScatteringMatrix::operator=(
		const PhScatteringMatrix & that) {
	ScatteringMatrix::operator=(that);
	if ( this != &that ) {
		coupling3Ph = that.coupling3Ph;
//		couplingIsotope = that.couplingIsotope;
//		couplingBoundary = that.couplingBoundary;
	}
	return *this;
}

// 3 cases:
// theMatrix and linedith is passed: we compute and store in memory the scatt
//       matrix and the diagonal
// inPopulation+outPopulation is passed: we compute the action of the
//       scattering matrix on the in vector, returning outVec = sMatrix*vector
// only linewidth is passed: we compute only the linewidths
void PhScatteringMatrix::builder(
		Eigen::MatrixXd * matrix, VectorBTE * linewidth,
		VectorBTE * inPopulation, VectorBTE * outPopulation) {

	int switchCase;
	if ( matrix != nullptr && linewidth != nullptr &&
		inPopulation == nullptr && outPopulation == nullptr  ) {
		switchCase = 0;
	} else if ( matrix == nullptr && linewidth == nullptr &&
		inPopulation != nullptr && outPopulation != nullptr  ) {
		switchCase = 1;
	} else if ( matrix == nullptr && linewidth != nullptr &&
		inPopulation == nullptr && outPopulation == nullptr  ) {
		switchCase = 2;
	} else {
		Error e("builder3Ph found a non-supported case");
	}

	if ( (linewidth != nullptr) && (linewidth->dimensionality!= 1) ) {
		Error e("The linewidths shouldn't have dimensionality");
	}

	bool dontComputeQ3 = &innerBandStructure == &outerBandStructure;

	auto statistics = outerBandStructure.getStatistics();
	double energyCutoff = 1.0e-8;

	long numAtoms = innerBandStructure.getPoints().getCrystal().getNumAtoms();

	// precompute Bose populations
	VectorBTE outerBose(context, outerBandStructure, 1);
	for ( long iCalc=0; iCalc<statisticsSweep.getNumCalcs(); iCalc++ ) {
		double temperature = statisticsSweep.getCalcStatistics(iCalc
				).temperature;
		for ( long iState=0; iState<numStates; iState++ ) {
			bose.data(iCalc,iState) = statistics.getPopulation(energy,
					temperature);
		}
	}
	VectorBTE innerBose(context, outerBandStructure, 1);
	if ( &innerBandStructure == &outerBandStructure ) {
		innerBose = outerBose;
	} else {
		for ( long iCalc=0; iCalc<statisticsSweep.getNumCalcs(); iCalc++ ) {
			double temperature = statisticsSweep.getCalcStatistics(iCalc
					).temperature;
			for ( long iState=0; iState<numStates; iState++ ) {
				bose.data(iCalc,iState) = statistics.getPopulation(energy,
						temperature);
			}
		}
	}

	// note: these variables are only needed in the loop
	// but since it's an expensive loop, we define them here once and for all
	double ratePlus1, ratePlus2, rateMins;
	double bose1, bose2, bose3Plus, bose3Mins;
	double deltaPlus1,deltaPlus2,deltaMins;

	Eigen::Tensor<double,3> couplingPlus, couplingMins;
	Eigen::VectorXd state3PlusEnergies, state3MinsEnergies;
	Eigen::VectorXd bose3Plus, bose3Mins;
	Eigen::Vector3d v1, v2;

	for( long iq1=0; iq1<numPoints; iq1++ ) {

		// note: for computing linewidths on a path, we must distinguish
		// that q1 and q2 are on different meshes, and that q3+/- may not fall
		// into known meshes and therefore needs to be computed

		auto states1 = outerBandStructure.getState(iq1);
		auto q1 = states1.getPoint();
		auto nb1 = states1.getNumBands();
		auto state1Energies = states1.getEnergies();

		for( long iq2=0; iq2<numPoints; iq2++ ) {
			auto q2 = innerBandStructure.getPoint(iq2);
			long iq2Inv = innerBandStructure.getPoints().getIndexInverse(iq2);
			auto q2Reversed = innerBandStructure.getPoint(iq2Inv);

			// note: + processes are phonon decay (1->2+3)
			// note: - processes are phonon coalescence (1+2->3)

			// we need the distinction, because the coupling for + process
			// must be computed at -q2 = q2Reversed
			auto states2 = innerBandStructure.getState(q2);
			auto state2Energies = states2.getEnergies();
			auto states2Plus = innerBandStructure.getState(q2Reversed);
			auto nb2Plus = states2Plus.getNumBands();
			auto nb2 = states2.getNumBands();
			if ( nb2Plus != nb2) {
				Error e("Unexpected nb2 in building the scattering matrix");
			}

			// if the meshes are the same (and gamma centered)
			// q3 will fall into the same grid, and it's easy to get
			if ( dontComputeQ3 ) {
				auto q3Plus = q1 + q2;
				auto q3Mins = q1 - q2;
				auto states3Plus = innerBandStructure.getState(q3Plus);
				auto states3Mins = innerBandStructure.getState(q3Mins);

				[couplingPlus, couplingMins] =
						coupling3Ph->getCouplingSquared(states1, states2Plus,
								states2Mins, states3Plus, states3Mins);
				state3PlusEnergies = states3Plus.getEnergies();
				state3MinsEnergies = states3Mins.getEnergies();

				for ( long ib3=0; ib3<nb3Plus; ib3++ ) {
					ind3 = state3Plus.getIndex(ib3);
					bose3PlusData.col(ib3) = outerBose.data.col(ind3);
				}
				for ( long ib3=0; ib3<nb3Mins; ib3++ ) {
					ind3 = state3Mins.getIndex(ib3);
					bose3MinsData.col(ib3) = outerBose.data.col(ind3);
				}
			} else {
				// otherwise, q3 doesn't fall into the same grid
				// and we must therefore compute it from the hamiltonian

				auto q3PlusC = q1.getCoords("cartesian")
						+ q2.getCoords("cartesian");
				auto q3MinsC = q1.getCoords("cartesian")
						- q2.getCoords("cartesian");
				auto q3Plus = Point<Eigen::Vector3d>(-1,
						Eigen::Vector3d::Zero(), q3PlusC);
				auto q3Mins = Point<Eigen::Vector3d>(-1,
						Eigen::Vector3d::Zero(), q3MinsC);
				auto [eigvals1,eigvecs1] = h0.diagonalizeFromCoords(q3Plus);
				State<Eigen::Vector3d> states3Plus(q3Plus, eigvals1, numAtoms,
						nb3Plus, nullptr, eigvecs1);

				auto [eigvals2,eigvecs2] = h0.diagonalizeFromCoords(q3Mins);
				State<Eigen::Vector3d> states3Mins(q3Mins, eigvals2, numAtoms,
						nb3Mins, nullptr, eigvecs2);

				[couplingPlus, couplingMins] =
						coupling3Ph->getCouplingSquared(states1, states2Plus,
								states2Mins, states3Plus, states3Mins);
				state3PlusEnergies = states3Plus.getEnergies();
				state3MinsEnergies = states3Mins.getEnergies();

				auto nb3Plus = state3PlusEnergies.size();
				auto nb3Mins = state3MinsEnergies.size();

				for ( long iCalc=0; iCalc<statisticsSweep.getNumCalcs();
						iCalc++ ) {
					double temperature = statisticsSweep.getCalcStatistics(
							iCalc).temperature;
					for ( long ib3=0; ib3<nb3Plus; ib3++ ) {
						bose3PlusData(iCalc,ib3) = statistics.getPopulation(
								state3PlusEnergies(ib3), temperature);
					}
					for ( long ib3=0; ib3<nb3Mins; ib3++ ) {
						bose3MinsData(iCalc,ib3) = statistics.getPopulation(
								state3MinsEnergies(ib3), temperature);
					}
				}
			}

			auto nb3Plus = state3PlusEnergies.size();
			auto nb3Mins = state3MinsEnergies.size();

			for ( long ib1=0; ib1<nb1; ib1++ ) {
				en1 = state1Energies(ib1);
				ind1 = state1.getIndex(ib1);

				for ( long ib2=0; ib2<nb2; ib2++ ) {
					en2 = state2Energies(ib2);
					ind2 = state2.getIndex(ib2);

					// split into two cases since there may be different bands
					for ( long ib3=0; ib3<nb3Plus; ib3++ ) {
						en3Plus = state3PlusEnergies(ib3);

						switch ( smearing.id ) {
						case ( DeltaFunction::gaussian ):
							deltaPlus1 = smearing.getSmearing(
									en1 + en3Plus - en2 );
							deltaPlus2 = smearing.getSmearing(
									en1 - en2 - en3Plus);
						case ( DeltaFunction::adaptiveGaussian ):
							v1 = state1.getVelocity(ib1);
							v2 = state2.getVelocity(ib2);
							deltaPlus1 = smearing.getSmearing(
									en1 + en3Plus - en2, v1-v2);
							deltaPlus2 = smearing.getSmearing(
									en1 - en2 - en3Plus, v1-v2);
						default:
							deltaPlus1 = smearing.getSmearing(
									en3Plus-en1, iq2, ib2);
							deltaPlus2 = smearing.getSmearing(
									en3Plus+en1, iq2, ib2);
						}

						// loop on temperature
						for ( long iCalc=0; iCalc<numCalcs; iCalc++ ) {

							bose1 = outerBose.data(iCalc,s1);
							bose2 = innerBose.data(iCalc,s2);
							bose3Plus = bose3Plusdata(iCalc,ib3);

							//Calculate transition probability W+
							ratePlus1 = pi * 0.25
									* bose3Plus * bose1 * ( bose2 + 1. )
									* couplingPlus(ib1,ib2,ib3)
									* deltaPlus;

							ratePlus2 = pi * 0.25
									* bose2 * bose3Plus * ( bose1 + 1. )
									* couplingPlus(ib1,ib2,ib3)
									* deltaPlus;

							// note: to increase performance, we are in fact
							// using

							switch ( switchCase ) {
							case (0):
								// case of matrix construction
								matrix(ind1,ind2) -= ratePlus1 + ratePlus2;
								linewidth.data(iCalc,ind1) +=
										0.5 * (ratePlus1 + ratePlus2);
							case (1):
								// case of matrix-vector multiplication

								for ( long i : {0,1,2} ) {
									outPopulation.data(3*iCalc+i,ind1) -=
											0.5 * (ratePlus1 + ratePlus2) *
											inPopulation.data(3*iCalc+i,ind2);
									outPopulation.data(3*iCalc+i,ind1) +=
											0.5 * (ratePlus1 + ratePlus2) *
											inPopulation.data(3*iCalc+i,ind1);
								}
							case (2):
								// case of linewidth construction
								linewidth.data(iCalc,ind1) +=
										0.5 * (ratePlus1 + ratePlus2);
							}
						}
					}

					for ( long ib3=0; ib3<nb3Mins; ib3++ ) {
						en3Plus = state3MinsEnergies(ib3);

//						deltaMins = fillTetsWeights(omega1-omega3Mins,s2,iq2,tetra);
						deltaMins = diracDelta.get(en1-en3Mins,state2);

						// gaussian smearing:
						deltaMins = (en1 + en2 - en3Mins) * sigmam1;
						deltaMins = exp( - deltaMins * deltaMins );

						for ( long iCalc=0; iCalc<numCalcs; iCalc++ ) {

							bose1 = bose.data(iCalc,s1);
							bose2 = bose.data(iCalc,s2);
							bose3Mins = bose3Minsdata(iCalc,ib3);

							//Calculatate transition probability W-
							rateMins = pi * 0.25
									* bose1 * bose2 * ( bose3Mins + 1. )
									* couplingMins(ib1,ib2,ib3)
									* deltaMins;

							matrix(i,j) += rateMins;
							outVector(i) += 0.5 * rateMins;

							switch ( switchCase ) {
							case (0):
								// case of matrix construction
								matrix(ind1,ind2) += rateMins;
								linewidth.data(iCalc,ind1) += 0.5 * rateMins;
							case (1):
								// case of matrix-vector multiplication
								for ( long i : {0,1,2} ) {
									outPopulation.data(3*iCalc+i,ind1) +=
											0.5 * rateMins *
											inPopulation.data(3*iCalc+i,ind2);
									outPopulation.data(3*iCalc+i,ind1) +=
											0.5 * rateMins *
											inPopulation.data(3*iCalc+i,ind1);
								}
							case (2):
								// case of linewidth construction
								linewidth.data(iCalc,ind1) += 0.5 * rateMins;
							}

						}
					}
				}
			}
		}
	}

	return outVector;
}
