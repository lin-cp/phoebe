#include <vector>
#include <complex>
#include <chrono>
#include <iostream>
#include "mpiController.h"
#include "Blacs.h"
#include "exceptions.h"
#include "context.h"

#ifdef MPI_AVAIL 
#include <mpi.h>
#endif

// default constructor
MPIcontroller::MPIcontroller() {
  // set default values for cases without MPI or blacs
  hasBlacs = false;
  blasRank_ = 0;
  blacsContext_ = 0;
  numBlasRows_ = 1;
  numBlasCols_ = 1;
  myBlasRow_ = 0;
  myBlasCol_ = 0;

#ifdef MPI_AVAIL
  // start the MPI environment
  int errCode = MPI_Init(NULL, NULL);
  if (errCode != MPI_SUCCESS) {
    errorReport(errCode);
  }

  // set this so that MPI returns errors and lets us handle them, rather
  // than using the default, MPI_ERRORS_ARE_FATAL
  MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

  // get the number of processes
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // get rank of current process
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // start a timer
  startTime = MPI_Wtime();

#else
  // To maintain consistency when running in serial
  size = 1;
  rank = 0;
  startTime = std::chrono::steady_clock::now();
#endif
}

// Initialized blacs for the cases where the scattering matrix is used
void MPIcontroller::initBlacs(const int& numBlasRows, const int& numBlasCols) {

#ifdef MPI_AVAIL
  if (!hasBlacs) {
    // initBlacs should only be called once.
    // by setting this to true, we prevent future calls
    hasBlacs = true;

    blacs_pinfo_(&blasRank_, &size);  // BLACS rank and world size
    int zero = 0;
    blacs_get_(&zero, &zero, &blacsContext_);  // -> Create context
 
    // kill the code if we asked for more blas rows/cols than there are procs
    if (getSize() < numBlasRows * numBlasCols) {
       Error e("initBlacs requested too many MPI processes.");
    } 

    // Cases for a blacs grid where we specified rows, cols, both, 
    // or the default, neither, which results in a square proc grid
    if(numBlasRows != 0 && numBlasCols == 0) {
        numBlasRows_ = numBlasRows; 
        numBlasCols_ = getSize()/numBlasRows; 
    } 
    else if(numBlasRows == 0 && numBlasCols != 0) {
        numBlasRows_ = getSize()/numBlasCols;
        numBlasCols_ = numBlasCols;  
    }  
    else if(numBlasRows !=0 && numBlasCols !=0 ) {
        numBlasRows_ = numBlasRows; 
        numBlasCols_ = numBlasCols; 
    } 
    else { // set up a square procs grid
      numBlasRows_ = (int)(sqrt(size));  // int does rounding down (intentional!)
      numBlasCols_ = numBlasRows_;       // scalapack requires square grid

      // we "pause" the processes that fall outside the blas grid
      if (size > numBlasRows_ * numBlasCols_) {
        Error e("Phoebe needs a square number of MPI processes");
      }
    }  

    blacs_gridinit_(&blacsContext_, &blacsLayout_, &numBlasRows_,
                    &numBlasCols_);
    // Context -> Context grid info (# procs row/col, current procs row/col)
    blacs_gridinfo_(&blacsContext_, &numBlasRows_, &numBlasCols_, &myBlasRow_,
                    &myBlasCol_);
  }
#endif
}

// TODO: any other stats would like to output here?
void MPIcontroller::finalize() const {
#ifdef MPI_AVAIL
  barrier();
  if (mpiHead()) {
    fprintf(stdout, "Final time: %3f\n ", MPI_Wtime() - startTime);
  }
  if(hasBlacs) blacs_gridexit_(&blacsContext_);
  MPI_Finalize();
#else
  std::cout << "Final time: "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - startTime)
                       .count() * 1e-6
            << " secs" << std::endl;
#endif
}

// Utility functions  -----------------------------------

// get the error string and print it to stderr before returning
void MPIcontroller::errorReport(int errCode) const{
	#ifdef MPI_AVAIL
	char errString[BUFSIZ];
	int lengthOfString;
	
	MPI_Error_string(errCode, errString, &lengthOfString);
	fprintf(stderr, "Error from rank %3d: %s\n", rank, errString);
	MPI_Abort(MPI_COMM_WORLD, errCode);
	#else 
	// TODO: how are we throwing non-mpi errors? 
	#endif
}

void MPIcontroller::time() const{
	#ifdef MPI_AVAIL
	fprintf(stdout, "Time for rank %3d : %3f\n", rank, MPI_Wtime() - startTime );
	#else
	std::cout << "Time for rank 0 :" << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count() << " secs" << std::endl;
	#endif
}
// Asynchronous support functions -----------------------------------------
void MPIcontroller::barrier() const{ 
#ifdef MPI_AVAIL
  int errCode;
  errCode = MPI_Barrier(MPI_COMM_WORLD);
  if(errCode != MPI_SUCCESS) {  errorReport(errCode); }
#endif
}

// Labor division functions -----------------------------------------
std::vector<int> MPIcontroller::divideWork(size_t numTasks) {
  // return a vector of the start and stop points for task division
  std::vector<int> divs(2); 
  divs[0] = (numTasks * rank)/size;
  divs[1] = (numTasks * (rank+1))/size;
  return divs; 
}

std::vector<int> MPIcontroller::divideWorkIter(size_t numTasks) {
  // return a vector of the start and stop points for task division
  std::vector<int> divs;
  int start = (numTasks * rank) / size;
  int stop = (numTasks * (rank + 1)) / size;
  for (int i = start; i < stop; i++) divs.push_back(i);
  return divs;
}

int MPIcontroller::getNumBlasRows() { return numBlasRows_; }

int MPIcontroller::getNumBlasCols() { return numBlasCols_; }

int MPIcontroller::getMyBlasRow() { return myBlasRow_; }

int MPIcontroller::getMyBlasCol() { return myBlasCol_; }

int MPIcontroller::getBlacsContext() { return blacsContext_; }

