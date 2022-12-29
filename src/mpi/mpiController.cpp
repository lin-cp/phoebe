#include "mpiController.h"
#include <chrono>
#include <iostream>
#include <vector>
#include "utilities.h"

#ifdef MPI_AVAIL
#include <mpi.h>
#endif

// default constructor
MPIcontroller::MPIcontroller(int argc, char *argv[]) {

#ifdef MPI_AVAIL
  // start the MPI environment
  int errCode = MPI_Init(nullptr, nullptr);
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

  // create groups of MPI processes

  int tmpPoolSize = 1;
  // first parse the poolSize from the command line
  for (int i=0; i<argc; i++) {
    if (std::string(argv[i]) == "-ps" || std::string(argv[i]) == "-poolSize") {
      if (i == argc-1) {
        Error("Error in correctly specifying poolSize on the command line");
      }
      hasMPIPools = true;
      bool isDigits = std::string(argv[i+1]).find_first_not_of("0123456789") == std::string::npos;
      if ( !isDigits ) {
        std::cout << "poolSize on command line has non-digits\n";
        exit(1);
      }
      tmpPoolSize = std::stoi(std::string(argv[i + 1]));
    }
  }
  if (tmpPoolSize == 0) {
    std::cout << "poolSize must be at least 1\n";
    exit(1);
  }
  if (mod(size, tmpPoolSize) != 0) {
    std::cout << "poolSize isn't an exact divisor of the # of MPI processes\n";
    exit(1);
  }

  // now split MPI processes in groups of size "poolSize"
  // https://mpitutorial.com/tutorials/introduction-to-groups-and-communicators/
  poolId = rank / tmpPoolSize; // Determine color based on row
  // Split the communicator based on the color and use the
  // original rank for ordering
  MPI_Comm_split(MPI_COMM_WORLD, poolId, rank, &intraPoolCommunicator);
  // initiate rank and size
  MPI_Comm_rank(intraPoolCommunicator, &poolRank);
  MPI_Comm_size(intraPoolCommunicator, &poolSize);

  // check results are as expected
  if ( poolSize != tmpPoolSize ) {
    std::cout << "Unexpected MPI split result\n";
    exit(1);
  }

  // for communications purposes, it's also useful to define a communicator
  // to send data across MPI processes with the same poolRank but across pools
  // (e.g. when reading data)
  int color = mod(rank, poolSize); // Determine color based on columns
  // Split the communicator based on the color and use the
  // original rank for ordering
  MPI_Comm_split(MPI_COMM_WORLD, color, rank, &interPoolCommunicator);

  // start a timer
  startTime = MPI_Wtime();

#else
  (void)argv;
  (void)argc;
  // To maintain consistency when running in serial
  size = 1;
  rank = 0;
  startTime = std::chrono::steady_clock::now();
  poolSize = 1;
  poolRank = 0;
#endif

  // Print the starting time
  if(mpiHead()) {
    // print date and time of run
    auto timeNow = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::cout << "Started on " << ctime(&timeNow);
  }
}

const int MPIcontroller::worldComm = worldComm_;
const int MPIcontroller::intraPoolComm = intraPoolComm_;
const int MPIcontroller::interPoolComm = interPoolComm_;

void MPIcontroller::finalize() const {
  if(mpiHead()) {
    // print date and time of run
    auto timenow = std::chrono::system_clock::to_time_t(
          std::chrono::system_clock::now());
    std::cout << "Finished on " << ctime(&timenow);
  }
#ifdef MPI_AVAIL
  barrier();
  if (mpiHead()) {
    fprintf(stdout, "Run time: %3f s\n", MPI_Wtime() - startTime);
  }
  MPI_Finalize();
#else
  std::cout << "Run time: "
            << std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - startTime)
                  .count() * 1e-6 << " s" << std::endl;
#endif
}

// Utility functions  -----------------------------------

// get the error string and print it to stderr before returning
void MPIcontroller::errorReport(int errCode) const {
#ifdef MPI_AVAIL
  char errString[BUFSIZ];
  int lengthOfString;

  MPI_Error_string(errCode, errString, &lengthOfString);
  fprintf(stderr, "Error from rank %3d: %s\n", rank, errString);
  MPI_Abort(MPI_COMM_WORLD, errCode);
#else
(void)errCode;
#endif
}

void MPIcontroller::time() const {
#ifdef MPI_AVAIL
  fprintf(stdout, "Time for rank %3d : %3f\n", rank, MPI_Wtime() - startTime);
#else
  std::cout << "Time for rank 0 :"
            << std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - startTime)
                   .count()
            << " secs" << std::endl;
#endif
}
// Asynchronous support functions -----------------------------------------
void MPIcontroller::barrier() const {
#ifdef MPI_AVAIL
  int errCode;
  errCode = MPI_Barrier(MPI_COMM_WORLD);
  if (errCode != MPI_SUCCESS) {
    errorReport(errCode);
  }
#endif
}

// Labor division functions -----------------------------------------
std::vector<size_t> MPIcontroller::divideWork(size_t numTasks) {
  // return a vector of the start and stop points for task division
  std::vector<size_t> divs(2);
  divs[0] = (numTasks * rank) / size;
  divs[1] = (numTasks * (rank + 1)) / size;
  return divs;
}

std::vector<size_t> MPIcontroller::divideWorkIter(size_t numTasks, const int& communicator) {
  int rank_ = 0;
  int size_ = 1;
  if (communicator == worldComm) {
    rank_ = rank;
    size_ = size;
  } else if (communicator == intraPoolComm) {
    rank_ = poolRank;
    size_ = poolSize;
  } else {
    Error("divideWorkIter called with invalid communicator");
  }

  // return a vector of indices for tasks to be completed by thsi process
  size_t start = (numTasks * rank_) / size_;
  size_t stop = (numTasks * (rank_ + 1)) / size_;
  size_t localSize = stop - start;
  std::vector<size_t> divs(localSize);
  for (size_t i = start; i < stop; i++) {
    divs[i - start] = i;
  }
  return divs;
}

// Helper function to re-establish work divisions for MPI calls requiring
// the number of tasks given to each point
std::tuple<std::vector<int>, std::vector<int>>
MPIcontroller::workDivHelper(size_t numTasks) const {

  std::vector<int> workDivs(size);
  // start points for each rank's work
  std::vector<int> workDivisionHeads(size);
  std::vector<int> workDivisionTails(size);
  // Recreate work division instructions
  for (int i = 0; i < size; i++) {
    workDivisionHeads[i] = (numTasks * i) / size;
    workDivisionTails[i] = (numTasks * (i + 1)) / size;
  }
  /** Note: it is important to compute workDivs as the subtraction of two
   * other variables. Some compilers (e.g. gcc 9.3.0 on Ubuntu) may optimize
   * the calculation of workDivs setting it to workDivs[i]=numTasks/size ,
   * which doesn't work when the division has a remainder.
   */
  for (int i = 0; i < size; i++) {
    workDivs[i] = workDivisionTails[i] - workDivisionHeads[i];
  }

  return std::make_tuple(workDivs, workDivisionHeads);
}

#ifdef MPI_AVAIL
std::tuple<MPI_Comm,int> MPIcontroller::decideCommunicator(const int& communicator) const {
  MPI_Comm comm = worldCommunicator;
  int broadcaster = 0;
  if (communicator == worldComm) {
    comm = worldCommunicator;
    broadcaster = mpiHeadId;
  } else if (communicator == intraPoolComm) {
    comm = intraPoolCommunicator;
    broadcaster = mpiHeadPoolId;
  } else if (communicator == interPoolComm) {
    comm = interPoolCommunicator;
    broadcaster = mpiHeadColsId;
  } else {
    Error("Invalid pool communicator");
  }
  return std::make_tuple(comm, broadcaster);
}
#endif


#ifdef MPI_AVAIL
//#define PASTE_BIGMPI_REDUCE_OP(OP,TYPE)
void BigMPI_SUM_CDOUBLE_x(void * invec, void * inoutvec, int * len, MPI_Datatype * bigtype)
{
    /* We are reducing a single element of bigtype... */
    assert(*len==1);

    int count; MPI_Status status;
    MPI_Get_elements(&status, MPI_DOUBLE_COMPLEX, &count);

    int c = (int)(count/INT_MAX);
    int r = (int)(count%INT_MAX);

    /* Can use typesize rather than extent here because all built-ins lack holes. */
    int typesize;
    MPI_Type_size(MPI_DOUBLE_COMPLEX, &typesize);
    for (int i=0; i<c; i++) {
        MPI_Reduce_local(invec+(size_t)i*INT_MAX*(size_t)typesize,
                          inoutvec+i*INT_MAX*(size_t)typesize,
                         INT_MAX, MPI_DOUBLE_COMPLEX, MPI_SUM);
    }
    MPI_Reduce_local(invec+(size_t)c*INT_MAX*(size_t)typesize,
                     inoutvec+c*INT_MAX*(size_t)typesize,
                     r, MPI_DOUBLE_COMPLEX, MPI_SUM);
    return;
}

/* Create a BigMPI_<op>_x */
//PASTE_BIGMPI_REDUCE_OP(SUM,DOUBLE)
//#undef PASTE_BIGMPI_REDUCE_OP
#endif

/* this should be an in-place allReduce */
//template <typename T>
/*void MPIcontroller::bigAllReduceSum(std::complex<double>* dataIn, const int& communicator) const {

  using namespace mpiContainer;
  #ifdef MPI_AVAIL

    int nRanks = getSize(communicator);
    int thisRank = getRank(communicator);
    auto tup = decideCommunicator(communicator);
    MPI_Comm comm = std::get<0>(tup);

    int version, subversion;
    MPI_Get_version(&version, &subversion);

    // if there's only one process we don't need to act
    if (nRanks == 1 || (communicator == intraPoolComm && poolSize == 1)) {
      return;
    }

    // if the size of the out array is less than INT_MAX,
    // we can just call regular all reduce ----------------------------
    size_t outSize = containerType<T>::getSize(dataIn);
    int errCode;
    if(outSize < INT_MAX) {

      // if size is less than INT_MAX, it's safe to store
      // these as ints and pass them directly to allReduce
      errCode = MPI_Allreduce(MPI_IN_PLACE,
                    containerType<std::complex<double>>::getAddress(dataIn),
                    containerType<std::complex<double>>::getSize(dataIn),
                    containerType<std::complex<double>>::getMPItype(), MPI_SUM, communicator);

      if (errCode != MPI_SUCCESS) errorReport(errCode);
      return;
    }
    else if(version < 3) { // this will have problems in mpi version <3
      std::cout << "You're running Phoebe with MPI version < 3.\n" <<
          "For this very large calculation, you manage to overflow some \n" <<
          "MPI calls. Please rebuild with version 3 or 4." << std::endl;
      Error e("Calculation overflows MPI, run with MPI version <3.");
    }

    // if the count number is too big for MPI int argument,
    // we do bigAllReduce instead ----------------------------------

    // set up the mpi container structure to receive the data
    MPI_Datatype container;
    datatypeHelper(&container, outSize, dataIn);

    // create the MPI operator for the user defined type
    MPI_Op containerSumOp;
    int commute;
    MPI_Op_commutative(MPI_SUM, &commute);
    // TODO use T to deal with this somehow
    MPI_Op_create(BigMPI_SUM_DOUBLE_x, commute, &containerSumOp);

    // call all reduce with user defined reduce op and container datatype
    errCode = MPI_Allreduce(containerType<T>::getAddress(dataIn),
              containerType<std::complex<double>>::getAddress(dataIn), 1, container,
              containerSumOp, comm);

    // free the datatype after use
    MPI_Type_free(&container);
    MPI_Op_free(&containerSumOp);

  #else
  (void)dataIn;
  (void)communicator;
  return;
  #endif
}*/

