#ifndef MPICONTROLLER_H
#define MPICONTROLLER_H

#include <vector>
#include <complex>
#include <chrono>
//#include <iostream>

#ifdef MPI_AVAIL 
#include <mpi.h>
#endif

class MPIcontroller{
	
	//MPI_Comm com; // MPI communicator -- might not need this, can just call default comm "MPI_COMM_WORLD"
	int size = 0; // number of MPI processses
	int rank;

	#ifdef MPI_AVAIL 
	double startTime; // the time for the entire mpi operation
	#else 
	std::chrono::steady_clock::time_point startTime; 
	#endif    

	public:
		// MPIcontroller class constructors -----------------------------------
		/** a constructor which sets up the MPI environment, initializes the communicator, and starts a timer **/
		MPIcontroller();
		~MPIcontroller(); 
		
		// Calls finalize and potentially reports statistics, time or handles errors
		void finalize() const; 
	
		// Collective communications functions -----------------------------------
		template<typename T> void bcast(T* dataIn) const;
		template<typename T> void reduceSum(T* dataIn, T* dataOut) const;
		template<typename T> void reduceMax(T* dataIn, T* dataOut) const;
		template<typename T> void reduceMin(T* dataIn, T* dataOut) const;
                template<typename T> void gatherv(T* dataIn, T* dataOut) const; 

		// point to point functions -----------------------------------
		//template<typename T> void send(T&& data) const;
		//template<typename T> void recv(T&& data) const;
	
		// Asynchronous functions
		void barrier() const;
	
		// Utility functions -----------------------------------
		bool mpiHead() const{ return rank==0; } // a function to tell us if this process is the head
		int getRank() const { return rank; }
		int getSize() const { return size; }    

		// Error reporting and statistics
		void errorReport(int errCode) const; // collect errors from processes and reports them, then kills the code
		void time() const; // returns time elapsed since mpi started
	
		// IO functions
		// TODO: implement these functions, if we need them. 
		void mpiWrite();
		void mpiRead();
		void mpiAppend(); 
			
                // Labor division helper functions
                void divideWork(size_t numTasks); // divide up a set of work 
                int workHead(); // get the first task assigned to a rank
                int workTail(); // get the last task assigned to a rank

        private: 
                // store labor division information
                std::vector<int> workDivisionHeads; // start points for each rank's work
                std::vector<int> workDivisionTails; // end points for each rank's work
};

// we need to use the concept of a "type traits" object to serialize the standard cpp types
// and then we can define specific implementations of this for types which are not standard
namespace mpiContainer {
        #ifdef MPI_AVAIL
        // Forward declaration for a basic container type 
        template <typename ...> struct containerType;

        // Define a macro to shorthand define this container for scalar types
        // Size for basic scalar types will always be 1
        #define MPIDataType(cppType,mpiType) \
        template<> struct containerType<cppType> { \
                        static inline cppType* getAddress(cppType* data) { return data; } \
                        static inline size_t getSize(cppType* data) { if(!data) return 0; return 1; } \
                        static inline MPI_Datatype getMPItype() { return mpiType; } \
        };

        // Use definition to generate containers for scalar types
        MPIDataType(int, MPI_INT)
        MPIDataType(unsigned int, MPI_UNSIGNED)
        MPIDataType(float, MPI_FLOAT)
        MPIDataType(double, MPI_DOUBLE)

        MPIDataType(std::complex<double>, MPI_DOUBLE)
        MPIDataType(std::complex<float>, MPI_FLOAT)
        #undef MPIDataType 

        // A container for a std::vector
        template<typename T> struct containerType<std::vector<T>>{
                        static inline T* getAddress(std::vector<T>* data) { return data->data(); }
                        static inline size_t getSize(std::vector<T>* data) { return data->size(); }
                        static inline MPI_Datatype getMPItype() { return containerType<T>::getMPItype(); }
        };
        // TODO: Define any other useful containers (a matrix? a standard array?)

        #endif
}

// Collective communications functions -----------------------------------
template<typename T> void MPIcontroller::bcast(T* dataIn) const{
        using namespace mpiContainer;
        #ifdef MPI_AVAIL
        if(size==1) return;
        int errCode;

        errCode = MPI_Bcast( containerType<T>::getAddress(dataIn), containerType<T>::getSize(dataIn), containerType<T>::getMPItype(), 0, MPI_COMM_WORLD);
        if(errCode != MPI_SUCCESS) {  errorReport(errCode); }
        #endif
}
template<typename T> void MPIcontroller::reduceSum(T* dataIn, T* dataOut) const{
        using namespace mpiContainer;
        #ifdef MPI_AVAIL
        if(size==1) return;
        int errCode;

        errCode = MPI_Reduce(containerType<T>::getAddress(dataIn),containerType<T>::getAddress(dataOut),containerType<T>::getSize(dataIn),containerType<T>::getMPItype(), MPI_SUM, 0, MPI_COMM_WORLD);
        if(errCode != MPI_SUCCESS) {  errorReport(errCode); }
        #endif
}
template<typename T> void MPIcontroller::reduceMax(T* dataIn, T* dataOut) const{
        using namespace mpiContainer;
        #ifdef MPI_AVAIL
        if(size==1) return;
        int errCode;

        errCode = MPI_Reduce(containerType<T>::getAddress(dataIn),containerType<T>::getAddress(dataOut),containerType<T>::getSize(dataIn),containerType<T>::getMPItype(), MPI_MAX, 0, MPI_COMM_WORLD);
        if(errCode != MPI_SUCCESS) {  errorReport(errCode); }
        #endif
}
template<typename T> void MPIcontroller::reduceMin(T* dataIn, T* dataOut) const{
        using namespace mpiContainer;
        #ifdef MPI_AVAIL
        if(size==1) return;
        int errCode;

        errCode = MPI_Reduce(containerType<T>::getAddress(dataIn),containerType<T>::getAddress(dataOut),containerType<T>::getSize(dataIn),containerType<T>::getMPItype(), MPI_MIN, 0, MPI_COMM_WORLD);
        if(errCode != MPI_SUCCESS) {  errorReport(errCode); }
        #endif
}
//int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
//               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)

template<typename T> void MPIcontroller::gatherv(T* dataIn, T* dataOut) const {
        using namespace mpiContainer; 
        #ifdef MPI_AVAIL
        int errCode; 
        std::vector<int> workDivs(size); 
        for(int i = 0; i<size; i++) workDivs[i] = workDivisionTails[i] - workDivisionHeads[i]; 

        errCode = MPI_Gatherv(containerType<T>::getAddress(dataIn),containerType<T>::getSize(dataIn),containerType<T>::getMPItype(),containerType<T>::getAddress(dataOut), workDivs.data(), workDivisionHeads.data(), containerType<T>::getMPItype(), 0, MPI_COMM_WORLD); 
        if(errCode != MPI_SUCCESS) {  errorReport(errCode); }
        #else
        dataOut = dataIn; // I think we can just switch the pointers. 
        #endif
}

#endif
