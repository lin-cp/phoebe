#include "ph_interaction_3ph.h"
#include "ifc3_parser.h"

using namespace std;

/*
complex<double> PhInteraction3Ph::calculateSingleV(const PhononTriplet &interactingPhonons,		\
				 const Eigen::MatrixXd &q,		\
				 const Eigen::Tensor<complex,3> &phononEigenvectors, \
				 const int numTriplets, const Eigen::tensor<double,4> &ifc3Tensor, \
				 const Eigen::Tensor<double,3> &cellPositions, \
				 Eigen::Tensor<int,2> &displacedAtoms,const CrystalInfo &crysInfo, \
				 char procType){*/
double PhInteraction3Ph::calculateSingleV(const PhononTriplet &interactingPhonons, \
				 const Eigen::MatrixXd &q,		\
				 const int numTriplets, const Eigen::Tensor<double,4> &ifc3Tensor, \
				 const Eigen::Tensor<double,3> &cellPositions, \
				 const Eigen::Tensor<int,2> &displacedAtoms,const CrystalInfo &crysInfo, \
				 char procType){
  
  if((procType != '+') && (procType != '-')){
    cout << "Character procType can only be '+' or '-'.";
    exit(-1);
  }
  
  complex<double> phase, V, V0;
  const std::complex<double> I(0, 1);
  int s1,s2,s3,it,ic,iDim,jDim,kDim,numAtoms,numBranches;
  double massNorm;
  Eigen::Vector3d q1,q2,q3;
  Eigen::Vector3d cell2Pos, cell3Pos;
  
  //Get vector of atomic species types
  //Eigen::VectorXi types = crystal.getAtomicSpecies();
  //Get masses of the atom types
  //Eigen::VectorXd masses = crystal.getSpeciesMasses();
  //numAtoms = crystal.getNumAtoms();
  //numBranches = 3*numAtoms; // number of phonon branches in 3d
  //Eigen::Tensor<complex double,3> ev1(3,numAtoms,numBranches) = state1.getEigenvectors();
  //Eigen::Tensor<complex double,3> ev2(3,numAtoms,numBranches) = state2.getEigenvectors();
  //Eigen::Tensor<complex double,3> ev3(3,numAtoms,numBranches) = state3.getEigenvectors();
  
  //For now I'm grabbing the following info from the PhononTriplet and CrystalInfo structs
  //defined below in the testing area. Later on I will connect to the Crystal and State classes. 
  Eigen::Tensor<complex <double>,3> ev1(3,crysInfo.numAtoms,crysInfo.numBranches); 
  Eigen::Tensor<complex <double>,3> ev2(3,crysInfo.numAtoms,crysInfo.numBranches);
  Eigen::Tensor<complex <double>,3> ev3(3,crysInfo.numAtoms,crysInfo.numBranches);
  
  ev1 = interactingPhonons.ev1;
  ev2 = interactingPhonons.ev2;
  ev3 = interactingPhonons.ev3;

  //phonon branches: s1,s2,s3
  s1 = interactingPhonons.s1;
  s2 = interactingPhonons.s2;
  s3 = interactingPhonons.s3;
  
  //Cartesian phonon wave vectors: q1,q2,q3
  q1 = q.row(interactingPhonons.iq1);
  q2 = q.row(interactingPhonons.iq2);
  q3 = q.row(interactingPhonons.iq3);
  
  for(it = 0; it < numTriplets; it++){// sum over all triplets
    for(ic = 0; ic < 3; ic++){
      cell2Pos(ic) = cellPositions(it,0,ic);
      cell3Pos(ic) = cellPositions(it,1,ic);
    }

    massNorm = sqrt(crysInfo.masses(crysInfo.types(displacedAtoms(it,0)))*	\
		    crysInfo.masses(crysInfo.types(displacedAtoms(it,1)))*	\
		    crysInfo.masses(crysInfo.types(displacedAtoms(it,2))));

    //Recall that the first primitive cell in the triplet is restricted to
    //the origin, so the phase for that cell is unity.
    if(procType == '+'){
      phase = exp(I*(q2.dot(cell2Pos)-q3.dot(cell3Pos)))/massNorm;
    }else if(procType == '-'){
      phase = exp(-I*(q2.dot(cell2Pos)+q3.dot(cell3Pos)))/massNorm;
    }
         
    V0 = complex<double>(0.0,0.0);
    
    for(iDim = 0; iDim < 3; iDim++){
      for(jDim = 0; jDim < 3; jDim++){
	for(kDim = 0; kDim < 3; kDim++){
	  if(procType == '+'){
	    V0 += ifc3Tensor(it,iDim,jDim,kDim)*	\
	      ev1(iDim,displacedAtoms(it,0),s1)*	\
	      ev2(jDim,displacedAtoms(it,1),s2)*	\
	      conj(ev3(kDim,displacedAtoms(it,2),s3));	    
	  }else if(procType == '-'){
	    V0 += ifc3Tensor(it,iDim,jDim,kDim)*	\
	      ev1(iDim,displacedAtoms(it,0),s1)*	\
	      conj(ev2(jDim,displacedAtoms(it,1),s2))*	\
	      conj(ev3(kDim,displacedAtoms(it,2),s3));
	  }
	}
      }
    }
    V0 *= phase;
  }

  /*
  if(procType == '+'){
    cout << "V+ = " << V0 << "\n";
  }else{
    cout << "V- = " << V0 << "\n";
  }
  */
  
  return abs(V0*conj(V0)); //in units of ?
}

// getV(const PhononMode mode, procType){
    // use symmetries to fetch from half q-space |V-|^{2} all |V-|^{2} and |V+|^{2} data.
//}


// * Function to calculate the minimal set of V_minus processes for a given IBZ mode
void PhInteraction3Ph::calculateIrredVminus(const int nq, const int grid[3], const PhononMode &mode, \
					    const Eigen::MatrixXi &indexMesh, const Eigen::MatrixXd &qFBZ, \
					    const Eigen::Tensor<complex<double>,3> &ev,const int numTriplets, \
					    const Eigen::Tensor<double,4> &ifc3Tensor, \
					    const Eigen::Tensor<double,3> &cellPositions, \
					    const Eigen::Tensor<int,2> &displacedAtoms, const CrystalInfo &crysInfo){

  int ix,iy,iz,iq1,iq2,iq3,ib,jb,s1,s2,s3;
  int i1x,i1y,i1z,i2x,i2y,i2z,i3x,i3y,i3z;
  Eigen::Vector3d q1,q2,q3;
  Eigen::Tensor<complex <double>,3> ev1(3,crysInfo.numAtoms,crysInfo.numBranches); 
  Eigen::Tensor<complex <double>,3> ev2(3,crysInfo.numAtoms,crysInfo.numBranches);
  Eigen::Tensor<complex <double>,3> ev3(3,crysInfo.numAtoms,crysInfo.numBranches);
  
  // Edge length of half (+1) space
  int nxHalf = grid[0]/2+1;
  int nyHalf = grid[1]/2+1;
  int nzHalf = grid[2]/2+1;

  cout << nxHalf << " " << nyHalf << " " << nzHalf << "\n";

  PhononTriplet interactingPhonons;
  PhInteraction3Ph phInt;

  int numAtoms = crysInfo.numAtoms;
  int numBranches = crysInfo.numBranches;
  double Vm2[numBranches*nxHalf*nyHalf*nzHalf];

  int count = 0;

  // Grab irred phonon mode info:
  iq1 = mode.iq; //index of wave vector in the full BZ
  s1 = mode.s; //branch
  
  q1 = qFBZ.row(iq1);

  //Demux 1st phonon wave vector
  //!!WARNING: For testing purposes using ShengBTE ordering!!!
  i1x = iq1%grid[0];
  i1y = (iq1/grid[0])%grid[1];
  i1z = iq1/grid[0]/grid[1];
  
  interactingPhonons.s1 = s1;
  interactingPhonons.iq1 = iq1; 
  for(int idim = 0; idim < 3; idim++){
    for(int iat = 0; iat < numAtoms; iat++){
      for(int ib = 0; ib < numBranches; ib++){
	ev1(idim,iat,ib) = ev(iq1,ib,jb);
      }
    }
  }
  interactingPhonons.ev1 = ev1;
  
  // Sum over half space of 2nd phonon wave vectors
  for(i2z = 0; i2z < nzHalf; i2z++){	
    for(i2y = 0; i2y < nyHalf; i2y++){
      for(i2x = 0; i2x < nxHalf; i2x++){
	//Muxed index of 2nd phonon wave vector
	//!!WARNING: For testing purposes using ShengBTE ordering!!!
	iq2 = (iz*grid[1] + iy)*grid[0] + ix;
	
	interactingPhonons.iq2 = iq2;
	for(int idim = 0; idim < 3; idim++){
	  for(int iat = 0; iat < numAtoms; iat++){
	    for(int ib = 0; ib < numBranches; ib++){
	      ev2(idim,iat,ib) = ev(iq2,ib,jb);
	    }
	  }
	}
	interactingPhonons.ev2 = ev2;

	// Third phonon wave vector (Umklapped, if needed)
	i3x = (i1x - i2x + grid[0])%grid[0];
	i3y = (i1y - i2y + grid[1])%grid[1];
	i3z = (i1z - i2z + grid[2])%grid[2];
	//!!WARNING: For testing purposes using ShengBTE ordering!!!
	iq3 = (i3z*grid[1] + i3y)*grid[0] + i3x;
	
	interactingPhonons.iq3 = iq3;
	for(int idim = 0; idim < 3; idim++){
	  for(int iat = 0; iat < numAtoms; iat++){
	    for(int ib = 0; ib < numBranches; ib++){
	      ev3(idim,iat,ib) = ev(iq3,ib,jb);
	    }
	  }
	}
	interactingPhonons.ev3 = ev3;
	
	// Sum over 2nd phonon branches
	for(ib = 0; ib < numBranches; ib++){
	  interactingPhonons.s2 = ib;
	  // Sum over 3rd phonon branches
	  for(jb = 0; jb < numBranches; jb++){
	    interactingPhonons.s3 = jb;
	    
	    // Call calculateSingleV
	    Vm2[count++] = phInt.calculateSingleV(interactingPhonons, qFBZ, numTriplets, ifc3Tensor, \
						  cellPositions, displacedAtoms, crysInfo, '-');

	    cout << "|V-|^2[(" << s1 << "," << iq1 << "), (" << ib << "," << iq2 << "), (" << jb << "," << iq3 << ")] = " \
		 << Vm2[count-1] << "\n";
	  }
	}
	//cout << "Calculated " << count << " V- processes.\n";
	//exit(-1);
      }    
    }
    cout << "Calculated " << count << " V- processes.\n";
  }
}
