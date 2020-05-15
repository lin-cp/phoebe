#include "ph_scattering.h"
#include "constants.h"

PhScatteringMatrix::PhScatteringMatrix(Context & context_,
			ActiveBandStructure & bandStructure_,
			Interaction3Ph * coupling3Ph_=nullptr) :
//			InteractionIsotope * couplingIsotope_=nullptr,
//			InteractionBoundary * couplingBoundary_=nullptr
			ScatteringMatrix(context_, bandStructure_) {
	coupling3Ph = coupling3Ph_,
//	couplingIsotope = couplingIsotope_,
//	couplingBoundary = couplingBoundary_) {
}

PhScatteringMatrix::PhScatteringMatrix(const PhScatteringMatrix & that) :
		ScatteringMatrix(that), coupling3Ph(that.coupling3Ph)
//		couplingIsotope(that.couplingIsotope);
//		couplingBoundary(that.couplingBoundary);
{
}

PhScatteringMatrix::PhScatteringMatrix & operator=(
		const PhScatteringMatrix & that) {
	ScatteringMatrix::operator=(that);
	if ( this != that ) {
		coupling3Ph = that.coupling3Ph;
//		couplingIsotope = that.couplingIsotope;
//		couplingBoundary = that.couplingBoundary;
	}
	return *this;
}


VectorBTE PhScatteringMatrix::builderManager(VectorBTE * inPopulation) {
	VectorBTE outVector(context, bandStructure);
	outVector.setConst(0.);
	if ( inPopulation == nullptr ) {
		// case in which we want to compute the diagonal only
		if ( couplingPh3 != nullptr ) outVector += internal3Ph(,);
	} else {
		if ( highMemory ) {
			if ( couplingPh3 != nullptr ) {
				outVector += builder3Ph(*theMatrix,);
			}
		} else { // compute on the fly
			if ( couplingPh3 != nullptr ) {
				outVector += builder3Ph(,*inPopulation);
			}
		}
	}
	return outVector;
}


//VectorBTE builderPhIsotope(Eigen::MatrixXd * theMatrix=nullptr,
//		VectorBTE * inPopulation=nullptr) {
//}
//
//VectorBTE builderPhBoundary(Eigen::MatrixXd * theMatrix=nullptr,
//		VectorBTE * inPopulation=nullptr) {
//}

// 3 cases:
// theMatrix is passed: we compute and store in memory the scatt matrix
//                      we return the diagonal
// inPopulation is passed: compute sMatrix * vector, matrix not kept in memory
//                      we return outVec = sMatrix*vector
// neither is passed: we compute and return the diagonal of the scatt matrix
VectorBTE PhScatteringMatrix::builder3Ph(Eigen::MatrixXd * matrix,
		VectorBTE * inPopulation) {

	VectorBTE outVector(context, bandStructure);

//	for ( i ) {
//		for ( j ) {
//			if ( inPopulation != nullptr ) {
//				outVector(i) = term * inPopulation(j);
//			if ( matrix != nullptr ) {
//				*theMatrix(i,j) += term;
//				outVector(i) += term; // cumulate the diagonal
//			} else {
//				outVector(i) += term; // cumulate the diagonal
//			}
//		}
//	}

	if ( ( matrix != nullptr || inPopulation != nullptr )
			&& innerBandStructure != outerBandStructure ) {
		Error e("To solve the BTE, we need equal inner/outer grids");
	}

	auto statistics = bandStructure.getStatistics();
	double energyCutoff = 1.0e-8;

	numAtoms = ...;

	DiracDelta diracDelta(deltaFunctionSelection, statistics);

	// precompute Bose populations
	PopulationVectorBTE bose(context, bandStructure);
	for ( long iCalc=0; iCalc<statisticsSweep.getNumCalcs(); iCalc++ ) {
		double temperature = statisticsSweep.getCalcStatistics(iCalc
				).temperature;
		for ( long iState=0; iState<numStates; iState++ ) {
			bose.data(iCalc,iState) = statistics.getPopulation(energy,
					temperature);
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
			long iq2Inv = bandStructure.getIndexInverse(iq2);
			auto q2Reversed = bandStructure.getPoint(iq2Inv);

			// note: + processes are phonon decay (1->2+3)
			// note: - processes are phonon coalescence (1+2->3)

			// we need the distinction, because the coupling for + process
			// must be computed at -q2 = q2Reversed
			auto states2 = bandStructure.getState(q2);
			auto state2Energies = states2.getEnergies();
			auto states2Plus = bandStructure.getState(q2Reversed);
			auto nb2Plus = states2Plus.getNumBands();
			auto nb2 = states2.getNumBands();
			if ( nb2Plus != nb2) {
				Error e("Unexpected nb2 in building the scattering matrix");
			}

			// if the meshes are the same (and gamma centered)
			// q3 will fall into the same grid, and it's easy to get
			if ( innerBandStructure == outerBandStructure ) {
				auto q3Plus = q1 + q2;
				auto q3Mins = q1 - q2;
				auto states3Plus = bandStructure.getState(q3Plus);
				auto states3Mins = bandStructure.getState(q3Mins);

				[couplingPlus, couplingMins] =
						coupling3Ph->getCouplingSquared(states1, states2Plus,
								states2Mins, states3Plus, states3Mins);
				state3PlusEnergies = states3Plus.getEnergies();
				state3MinsEnergies = states3Mins.getEnergies();

				for ( long ib3=0; ib3<nb3Plus; ib3++ ) {
					ind3 = ;
					bose3PlusData.col(ib3) = bose.data.col(ind3);
				}
				for ( long ib3=0; ib3<nb3Mins; ib3++ ) {
					ind3 = ;
					bose3MinsData.col(ib3) = bose.data.col(ind3);
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
				ind1 =;

				for ( long ib2=0; ib2<nb2; ib2++ ) {
					en2 = state2Energies(ib1);
					ind2 =;

					// split into two cases since there may be different bands
					for ( long ib3=0; ib3<nb3Plus; ib3++ ) {
						en3Plus = state3PlusEnergies(ib1);
						ind3 = ;

//						deltaPlus = fillTetsWeights(omega3Plus-omega1,s2,iq2,tetra);
//						deltaPlus = diracDelta(state1,state2,state3Plus);

						// gaussian smearing:
						deltaPlus1 = (en1 + en3Plus - en2 ) * sigmam1;
						deltaPlus1 = exp( - deltaPlus1 * deltaPlus1 );

						deltaPlus2 = (en1 - en2 - en3Plus) * sigmam1;
						deltaPlus2 = exp( - deltaPlus2 * deltaPlus2 );

						// loop on temperature
						for ( long iCalc=0; iCalc<numCalcs; iCalc++ ) {

							bose1 = bose.data(iCalc,s1);
							bose2 = bose.data(iCalc,s2);
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
							if ( matrix != nullptr ) {
								// case of matrix construction
								matrix(ind1,ind2) -= ratePlus1 + ratePlus2;
								outVector.data(iCalc,ind1) +=
										0.5 * (ratePlus1 + ratePlus2);
							} else if ( inPopulation != nullptr) {
								// case of matrix-vector multiplication

								I miss a loop on the cartesian dir in iCart!

								for ( long i : {0,1,2} ) {
									outVector.data(3*iCalc+i,ind1) -=
											0.5 * (ratePlus1 + ratePlus2) *
											inPopulation.data(3*iCalc+i,ind2);
									outVector.data(3*iCalc+i,ind1) +=
											0.5 * (ratePlus1 + ratePlus2) *
											inPopulation.data(3*iCalc+i,ind1);
								}
							} else {
								// case of linewidth construction
								outVector.data(iCalc,ind1) +=
										0.5 * (ratePlus1 + ratePlus2);
							}
						}
					}

					for ( long ib3=0; ib3<nb3Mins; ib3++ ) {
						en3Plus = state3MinsEnergies(ib3);
						ind3 = ;

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
						}
					}
				}
			}
		}
	}

	return outVector;
}
