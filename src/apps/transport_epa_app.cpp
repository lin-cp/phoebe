#include "transport_epa_app.h"
#include "bandstructure.h"
#include "constants.h"
#include "context.h"
#include "delta_function.h"
#include "eigen.h"
#include "exceptions.h"
#include "interaction_epa.h"
#include "io.h"
#include "onsager.h"
#include "particle.h"
#include "qe_input_parser.h"
#include "statistics_sweep.h"
#include "utilities.h"
#include "vector_bte.h"
#include <math.h>

void TransportEpaApp::run(Context &context) {

  double fermiLevel = context.getFermiLevel();
  if (std::isnan(fermiLevel)) {
    Error e("Fermi energy must be provided for EPA calculation");
  }
  // Read necessary input: xml file of QE.
  // name of xml file should be provided in the input
  // electronFourierCutoff should be provided in the input (should it be the
  // same as encut in DFT?) Crystal crystal(directUnitCell, atomicPositions,
  // atomicSpecies, speciesNames, speciesMasses, dimensionality);
  // ElectronH0Fourier electronH0(crystal, coarsePoints, coarseBandStructure,
  // fourierCutoff);

  auto t1 = QEParser::parseElHarmonicFourier(context);
  auto crystal = std::get<0>(t1);
  auto electronH0 = std::get<1>(t1);

  // Read and setup k-point mesh for interpolating bandstructure
  FullPoints fullPoints(crystal, context.getKMesh());
  bool withVelocities = true;
  bool withEigenvectors = true;

  // Fourier interpolation of the electronic band structure
  FullBandStructure bandStructure =
      electronH0.populate(fullPoints, withVelocities, withEigenvectors);

  Particle particle = bandStructure.getParticle();

  // set temperatures, chemical potentials and carrier concentrations
  StatisticsSweep statisticsSweep(context, &bandStructure);

  //--------------------------------
  // Setup energy grid

  double minEnergy = fermiLevel - context.getEnergyRange();
  double maxEnergy = fermiLevel + context.getEnergyRange();
  double energyStep = context.getEnergyStep();
  // in principle, we should add 1 to account for ends of energy interval
  // i will not do that, because will work with the centers of energy steps
  long numEnergies = long((maxEnergy - minEnergy) / energyStep);
  if (mpi->mpiHead()) {
    std::cout << "Num energies: " << numEnergies << std::endl;
  }
  // energies at the centers of energy steps
  Eigen::VectorXd energies(numEnergies);
  for (long i = 0; i < numEnergies; ++i) {
    // add 0.5 to be in the middle of the energy step
    energies(i) = (i + 0.5) * energyStep + minEnergy;
  }

  //--------------------------------
  // Calculate EPA scattering rates
  BaseVectorBTE scatteringRates =
      getScatteringRates(context, statisticsSweep, bandStructure, energies);

  //--------------------------------
  // calc EPA velocities
  auto energyProjVelocity =
      calcEnergyProjVelocity(context, bandStructure, energies);

  //--------------------------------
  // compute transport coefficients
  OnsagerCoefficients transCoeffs(statisticsSweep, crystal, bandStructure,
                                  context);

  transCoeffs.calcFromEPA(scatteringRates, energyProjVelocity, energies,
                          energyStep, particle);

  transCoeffs.calcTransportCoefficients();
  transCoeffs.print();
}

// void TransportEpaApp::checkRequirements(Context & context) {
//    throwErrorIfUnset(context.getEpaEFileName(), "epaEFileName");
//    throwErrorIfUnset(context.getElectronH0Name(), "electronH0Name");
//    throwErrorIfUnset(context.getQMesh(), "kMesh");
//    throwErrorIfUnset(context.getDosMinEnergy(), "dosMinEnergy");
//    throwErrorIfUnset(context.getDosMaxEnergy(), "dosMaxEnergy");
//    throwErrorIfUnset(context.getDosDeltaEnergy(), "dosDeltaEnergy");
//    throwErrorIfUnset(context.getElectronFourierCutoff(),
//                      "electronFourierCutoff");
//}

void foldWithinBounds(int &idx, const int &numBins) {
  if (idx < 0) {
    idx = 0;
  }
  if (idx > numBins) {
    idx = numBins - 1;
  }
}

Eigen::Tensor<double, 3>
TransportEpaApp::calcEnergyProjVelocity(Context &context,
                                        BaseBandStructure &bandStructure,
                                        const Eigen::VectorXd &energies) {

  int numEnergies = energies.size();
  long numStates = bandStructure.getNumStates();
  int numPoints = bandStructure.getNumPoints(true);
  int dim = context.getDimensionality();

  Eigen::Tensor<double, 3> energyProjVelocity(dim, dim, numEnergies);
  energyProjVelocity.setZero();

  TetrahedronDeltaFunction tetrahedra(bandStructure);

  if (mpi->mpiHead()) {
    std::cout << "Calculating energy projected velocity tensor" << std::endl;
  }

  for (long iEnergy = 0; iEnergy != numEnergies; ++iEnergy) {
    for (long iState = 0; iState != numStates; ++iState) {
      auto t = bandStructure.getIndex(iState);
      int ik = std::get<0>(t).get();
      int ib = std::get<1>(t).get();
      Eigen::Vector3d velocity = bandStructure.getGroupVelocity(iState);
      double deltaFunction = tetrahedra.getSmearing(energies(iEnergy), ik, ib);
      for (int j = 0; j < dim; ++j) {
        for (int i = 0; i < dim; ++i) {
          energyProjVelocity(i, j, iEnergy) +=
              velocity(i) * velocity(j) * deltaFunction / numPoints;
        }
      }
    }
  }

  return energyProjVelocity;
}

BaseVectorBTE TransportEpaApp::getScatteringRates(
    Context &context, StatisticsSweep &statisticsSweep,
    FullBandStructure &fullBandStructure, Eigen::VectorXd &energies) {

  long numStates = fullBandStructure.getNumStates();

  /*If constant relaxation time is specified in input, we don't need to
  calculate EPA lifetimes*/
  double constantRelaxationTime = context.getConstantRelaxationTime();
  if (constantRelaxationTime > 0.) {
    BaseVectorBTE crtRate(statisticsSweep, numStates, 1);
    crtRate.setConst(1. / constantRelaxationTime);
    return crtRate;
  }

  auto hasSpinOrbit = context.getHasSpinOrbit();
  int spinFactor = 2;
  if (hasSpinOrbit)
    spinFactor = 1;

  auto particle = Particle(Particle::electron);
  auto phParticle = Particle(Particle::phonon);

  if (particle.isPhonon())
    Error e("Electronic bandstructure has to be provided");

  long numCalcs = statisticsSweep.getNumCalcs();

  std::cout << "\nCalculate electronic density of states." << std::endl;
  TetrahedronDeltaFunction tetrahedra(fullBandStructure);

  long numEnergies = energies.size();
  double energyStep = context.getEnergyStep();

  // in principle, we should add 1 to account for ends of energy interval
  // i will not do that, because will work with the centers of energy steps
  //    long numEnergies = (long) (maxEnergy-minEnergy)/energyStep;

  // calculate the density of states at the energies in energies vector
  Eigen::VectorXd dos(numEnergies);
  for (long i = 0; i != numEnergies; ++i) {
    dos(i) = tetrahedra.getDOS(energies(i));
  }

  // get vector containing averaged phonon frequencies per mode
  InteractionEpa couplingEpa = InteractionEpa::parseEpaCoupling(context);

  Eigen::VectorXd phEnergies = couplingEpa.getPhEnergies();
  int numPhEnergies = phEnergies.size();

  // phJump describes how bin-jumps the electron does after scattering
  // as a double
  Eigen::VectorXd phJump(numPhEnergies);
  for (auto i = 0; i != phEnergies.size(); ++i) {
    phJump(i) = phEnergies(i) / energyStep;
  }

  Eigen::VectorXd elphEnergies = couplingEpa.getElEnergies();
  double minElphEnergy = elphEnergies(0);
  int numElphBins = elphEnergies.size();
  double binSize = 1.;
  if (numElphBins > 1) {
    binSize = elphEnergies(1) - elphEnergies(0);
  }

  Eigen::Tensor<double, 3> elPhMatElements = couplingEpa.getElPhMatAverage();

  LoopPrint loopPrint("calculation of EPA scattering rates", "energies",
                      mpi->divideWorkIter(numEnergies).size());

  BaseVectorBTE epaRate(statisticsSweep, numEnergies, 1);

  // loop over temperatures and chemical potentials
  // loop over energies
#pragma omp parallel
  {
    Eigen::MatrixXd privateRates(numCalcs, numStates);
    privateRates.setZero();

#pragma omp for nowait
    for (long iEnergy : mpi->divideWorkIter(numEnergies)) {
      loopPrint.update();

      for (long iCalc = 0; iCalc < numCalcs; ++iCalc) {
        double temp = statisticsSweep.getCalcStatistics(iCalc).temperature;
        double chemPot =
            statisticsSweep.getCalcStatistics(iCalc).chemicalPotential;

        // loop over phonon frequencies
        for (int iPhFreq = 0; iPhFreq < numPhEnergies; ++iPhFreq) {

          // Avoid some index out of bound errors
          if (iEnergy + phJump(iPhFreq) + 1 >= numEnergies ||
              iEnergy - phJump(iPhFreq) - 1 < 0) {
            continue;
          }

          // population of phonons, electron after emission/absorption
          double nBose = phParticle.getPopulation(phEnergies(iPhFreq), temp);
          double nFermiAbsorption = particle.getPopulation(
              energies[iEnergy] + phEnergies(iPhFreq), temp, chemPot);
          double nFermiEmission = particle.getPopulation(
              energies[iEnergy] - phEnergies(iPhFreq), temp, chemPot);

          // compute the dos for electron in the final state for the two
          // scatterings mechanisms
          // Note: we do a linear interpolation
          int iJump = (int)phJump(iPhFreq);
          double iInterp = phJump(iPhFreq) - (double)iJump;
          double dosAbsorption =
              dos(iEnergy + iJump) * (1. - iInterp) +
              dos(iEnergy + iJump + 1) * iInterp;
          double dosEmission = dos(iEnergy - iJump - 1) * iInterp +
                               dos(iEnergy - iJump) * (1. - iInterp);

          // find index of the energy in the bins of the elph energies
          int intBinPos =
              int(std::round((energies(iEnergy) - minElphEnergy) / binSize));
          int iAbsInt = int(std::round(
              (energies(iEnergy) + phEnergies(iPhFreq) - minElphEnergy) /
              binSize));
          int iEmisInt = int(std::round(
              (energies(iEnergy) - phEnergies(iPhFreq) - minElphEnergy) /
              binSize));
          // check and fold within bounds:
          foldWithinBounds(intBinPos, numElphBins);
          foldWithinBounds(iAbsInt, numElphBins);
          foldWithinBounds(iEmisInt, numElphBins);

          //------------------------------------
          // estimate strength of el-ph coupling |g|^2

          double gAbsorption = elPhMatElements(iPhFreq, iAbsInt, intBinPos);
          double gEmission = elPhMatElements(iPhFreq, iEmisInt, intBinPos);

          //-----------------------------
          // finally, the scattering rate

          privateRates(iCalc, iEnergy) +=
              twoPi / spinFactor * gAbsorption * (nBose + nFermiAbsorption) *
                  dosAbsorption +
              gEmission * (nBose + 1 - nFermiEmission) * dosEmission;
        }
      }
    }

#pragma omp critical
    {
      for (long iEnergy = 0; iEnergy < numEnergies; ++iEnergy) {
        for (long iCalc = 0; iCalc < numCalcs; ++iCalc) {
          epaRate.data(iCalc, iEnergy) += privateRates(iCalc, iEnergy);
        }
      }
    }
  }
  mpi->allReduceSum(&epaRate.data);
  loopPrint.close();
  return epaRate;
}
