/** @file

    API of Par4All C to OpenMP

    This file is an implementation of the C to accelerator Par4All API.

    Funded by the FREIA (French ANR), TransMedi\@ (French Péle de
    Compétitivité Images and Network) and SCALOPES (Artemis European
    Project project)

    "mailto:Stephanie.Even@enstb.org"
    "mailto:Ronan.Keryell@hpc-project.com"
*/

#include <p4a_accel.h>
#include <errno.h>

/**
 * Flag that trigger timing of kernel execution
 */
int p4a_timing = 0;

/**
 *  This is used by P4A_TIMING routine to compute elapsed time for a kernel
 *  execution (in ms)
 */
float p4a_timing_elapsedTime = -1;
struct timeval p4a_time_begin, p4a_time_end;

/**
 * The main debug level for p4a
 */
int p4a_debug_level = 0;

void p4a_main_init() {
  checkStackSize();

  /* Debug level stuff */
  char *env_p4a_debug= getenv ("P4A_DEBUG");
  if(env_p4a_debug) {
    printf("%s",env_p4a_debug);
    errno = 0;
    int debug_level = strtol(env_p4a_debug, NULL,10);
    if(errno==0) {
      P4A_dump_message("Setting debug level to %d\n",debug_level);
      p4a_debug_level= debug_level;
    } else {
      fprintf(stderr,"Invalid value for P4A_DEBUG: %s\n",env_p4a_debug);
    }
  }

  /* Timing stuff */
  char *env_p4a_timing= getenv ("P4A_TIMING");
  if(env_p4a_timing) {
    printf("%s",env_p4a_timing);
    errno = 0;
    int timing = strtol(env_p4a_timing, NULL,10);
    if(errno==0) {
      P4A_dump_message("Enable timing (%d)\n",timing);
      p4a_timing= timing;
    } else {
      fprintf(stderr,"Invalid value for P4A_TIMING: %s\n",env_p4a_timing);
    }
  }
}


#ifdef P4A_ACCEL_OPENMP

void p4a_init_openmp_accel() {
  p4a_main_init();
}

/**
 * global structure used for timing purpose
 */




/** Stop a timer on the accelerator and get double ms time

    @addtogroup P4A_OpenMP_time_measure
*/
double P4A_accel_timer_stop_and_float_measure() {
  double run_time;
  gettimeofday(&p4a_time_end, NULL);
  /* Take care of the non-associativity in floating point :-) */
  run_time = (p4a_time_end.tv_sec - p4a_time_begin.tv_sec)
    + (p4a_time_end.tv_usec - p4a_time_begin.tv_usec)*1e-6;
  return run_time;
}


/** This is a global variable used to simulate P4A virtual processor
    coordinates in OpenMP because we need to pass a local variable to a
    function without passing it in the arguments.

    Use thead local storage to have it local to each OpenMP thread.

    With __thread, it looks like this declaration cannot be repeated in
    the .h without any extern.
 */
__thread int P4A_vp_coordinate[P4A_vp_dim_max];


/** @defgroup P4A_OpenMP_memory_allocation_copy Memory allocation and copy for OpenMP simulation

    @{
*/

/** Allocate memory on the hardware accelerator in OpenMP emulation.

    For OpenMP it is on the host too.

    @param[out] address is the address of a variable that is updated by
    this macro to contains the address of the allocated memory block

    @param[in] size is the size to allocate in bytes
*/
void P4A_accel_malloc(void **address, size_t size) {
  *address = malloc(size);
}


/** Free memory on the hardware accelerator in OpenMP emulation.

    It is on the host too

    @param[in] address points to a previously allocated memory zone for
    the hardware accelerator
*/
void P4A_accel_free(void *address) {
  free(address);
}


/** Copy a scalar from the hardware accelerator to the host in OpenMP
 emulation.

 Since it is an OpenMP implementation, use standard memory copy
 operations

 Do not change the place of the pointers in the API. The host address
 is always first...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_from_accel(size_t element_size,
    void *host_address,
    const void *accel_address) {
  /* We can use memcpy() since we are sure there is no overlap */
  memcpy(host_address, accel_address, element_size);
}

/** Copy a scalar from the host to the hardware accelerator in OpenMP
 emulation.

 Since it is an OpenMP implementation, use standard memory copy
 operations

 Do not change the place of the pointers in the API. The host address
 is always before the accel address...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_to_accel(size_t element_size,
    const void *host_address,
    void *accel_address) {
  /* We can use memcpy() since we are sure there is no overlap */
  memcpy(accel_address, host_address, element_size);
}

/* @} */
#endif // P4A_ACCEL_OPENMP

#ifdef P4A_ACCEL_CUDA


int p4a_max_threads_per_block = P4A_CUDA_THREAD_MAX;

void p4a_init_cuda_accel() {
  // Main p4a initialization
  p4a_main_init();

  // Timing stuff
  toolTestExec(cudaEventCreate(&p4a_start_event));
  toolTestExec(cudaEventCreate(&p4a_stop_event));

  // Threads per blocks
  char *env_p4a_max_tpb = getenv ("P4A_MAX_TPB");
  if(env_p4a_max_tpb) {
    errno = 0;
    int tpb = strtol(env_p4a_max_tpb, NULL,10);
    if(errno==0) {
      P4A_dump_message("Setting max TPB to %d\n",tpb);
      p4a_max_threads_per_block = tpb;
    } else {
      fprintf(stderr,"Invalid value for P4A_MAX_TPB : %s\n",env_p4a_max_tpb);
    }
  }

  // We prefer to have more L1 cache and less shared since we don't make use of it
  cudaDeviceSetCacheConfig( cudaFuncCachePreferL1 );
  toolTestExecMessage("P4A CUDA cache config failed");

}



/** To do basic time measure. Do not nest... */

cudaEvent_t p4a_start_event, p4a_stop_event;

/** Stop a timer on the accelerator and get float time in second

 @addtogroup P4A_cuda_time_measure
 */
double P4A_accel_timer_stop_and_float_measure() {
  float execution_time;
  toolTestExec(cudaEventRecord(p4a_stop_event, 0));
  toolTestExec(cudaEventSynchronize(p4a_stop_event));
  /* Get the time in ms: */
  toolTestExec(cudaEventElapsedTime(&execution_time,
          p4a_start_event,
          p4a_stop_event));
  /* Return the time in second: */
  return execution_time*1e-3;
}



/** Allocate memory on the hardware accelerator in Cuda mode.

    @param[out] address is the address of a variable that is updated by
    this function to contains the address of the allocated memory block

    @param[in] size is the size to allocate in bytes
*/
void P4A_accel_malloc(void **address, size_t size) {
  toolTestExec(cudaMalloc(address, size));
}


/** Free memory on the hardware accelerator in Cuda mode

    @param[in] address points to a previously allocated memory zone for
    the hardware accelerator
*/
void P4A_accel_free(void *address) {
  toolTestExec(cudaFree(address));
}


/** Copy a scalar from the hardware accelerator to the host

 It's a wrapper around CudaMemCopy*.

 Do not change the place of the pointers in the API. The host address
 is always first...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_from_accel(size_t element_size,
    void *host_address,
    void const*accel_address) {
  if(p4a_timing) {
    P4A_TIMING_accel_timer_start;
  }

  cudaMemcpy(host_address,accel_address,element_size,cudaMemcpyDeviceToHost);

  if(p4a_timing) {
    P4A_TIMING_accel_timer_stop;
    P4A_TIMING_elapsed_time(p4a_timing_elapsedTime);
    P4A_dump_message("Copied %zd bytes of memory from accel %p to host %p : "
                     "%.1fms - %.2fGB/s\n",element_size, accel_address,host_address,
                      p4a_timing_elapsedTime,(float)element_size/(p4a_timing_elapsedTime*1000000));

  }
}

/** Copy a scalar from the host to the hardware accelerator

 It's a wrapper around CudaMemCopy*.

 Do not change the place of the pointers in the API. The host address
 is always before the accel address...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_to_accel(size_t element_size,
    void const*host_address,
    void *accel_address) {
  if(p4a_timing) {
    P4A_TIMING_accel_timer_start;
  }

  cudaMemcpy(accel_address,host_address,element_size,cudaMemcpyHostToDevice);

  if(p4a_timing) {
    P4A_TIMING_accel_timer_stop;
    P4A_TIMING_elapsed_time(p4a_timing_elapsedTime);
    P4A_dump_message("Copied %zd bytes of memory from host %p to accel %p : "
                      "%.1fms - %.2fGB/s\n",element_size, host_address,accel_address,
                      p4a_timing_elapsedTime,(float)element_size/(p4a_timing_elapsedTime*1000000));
  }
}




#endif //P4A_ACCEL_CUDA


#ifdef P4A_ACCEL_OPENCL

#include <string.h>

/** @author Stéphanie Even

    Aside the procedure defined for CUDA or OpenMP, OpenCL needs
    loading the kernel from a source file.

    Procedure to manage a kernel list have thus been created for the C version.
    In C++, this a map.

    C and C++ versions are implemented.
 */


/** @defgroup P4A_log Debug messages and errors tracking.
    
    @{
*/

cl_int p4a_global_error=0;

cl_device_id p4a_device_id=0;
static cl_program p4a_program=0;

#define P4A_DEBUG_BUFFER_SIZE 10000
static char p4a_debug_buffer[P4A_DEBUG_BUFFER_SIZE];

/** @author : Stéphanie Even

    In OpenCL, errorToString doesn't exists by default ...
    Macros have been picked from cl.h
 */
char * p4a_error_to_string(int error) 
{
  switch (error)
    {
    case CL_SUCCESS:
      return (char *)"P4A : Success";
    case CL_DEVICE_NOT_FOUND:
      return (char *)"P4A : Device Not Found";
    case CL_DEVICE_NOT_AVAILABLE:
      return (char *)"P4A : Device Not Available";
    case CL_COMPILER_NOT_AVAILABLE:
      return (char *)"P4A : Compiler Not Available";
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
      return (char *)"P4A : Mem Object Allocation Failure";
    case CL_OUT_OF_RESOURCES:
      return (char *)"P4A : Out Of Ressources";
    case CL_OUT_OF_HOST_MEMORY:
      return (char *)"P4A : Out Of Host Memory";
    case CL_PROFILING_INFO_NOT_AVAILABLE:
      return (char *)"P4A : Profiling Info Not Available";
    case CL_MEM_COPY_OVERLAP:
      return (char *)"P4A : Mem Copy Overlap";
    case CL_IMAGE_FORMAT_MISMATCH:
      return (char *)"P4A : Image Format Mismatch";
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
      return (char *)"P4A : Image Format Not Supported";
    case CL_BUILD_PROGRAM_FAILURE:
	#define CL_BUILD_PROGRAM_FAILURE_MSG "P4A : Build Program Failure : "
	strncat(p4a_debug_buffer,CL_BUILD_PROGRAM_FAILURE_MSG,P4A_DEBUG_BUFFER_SIZE);
	clGetProgramBuildInfo( 	p4a_program,
  				p4a_device_id,
  				CL_PROGRAM_BUILD_LOG ,
			  	P4A_DEBUG_BUFFER_SIZE,
			  	p4a_debug_buffer+strlen(CL_BUILD_PROGRAM_FAILURE_MSG),
			  	NULL);
      return (char *)p4a_debug_buffer;
    case CL_MAP_FAILURE:
      return (char *)"P4A : Map Failure";
    case CL_INVALID_VALUE:
      return (char *)"P4A : Invalid Value";
    case CL_INVALID_DEVICE_TYPE:
      return (char *)"P4A : Invalid Device Type";
    case CL_INVALID_PLATFORM:
      return (char *)"P4A : Invalid Platform";
    case CL_INVALID_DEVICE:
      return (char *)"P4A : Invalid Device";
    case CL_INVALID_CONTEXT:
      return (char *)"P4A : Invalid Context";
    case CL_INVALID_QUEUE_PROPERTIES:
      return (char *)"P4A : Invalid Queue Properties";
    case CL_INVALID_COMMAND_QUEUE:
      return (char *)"P4A : Invalid Command Queue";
    case CL_INVALID_HOST_PTR:
      return (char *)"P4A : Invalid Host Ptr";
    case CL_INVALID_MEM_OBJECT:
      return (char *)"P4A : Invalid Mem Object";
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
      return (char *)"P4A : Invalid Image Format Descriptor";
    case CL_INVALID_IMAGE_SIZE:
      return (char *)"P4A : Invalid Image Size";
    case CL_INVALID_SAMPLER:
      return (char *)"P4A : Invalid Sampler";
    case CL_INVALID_BINARY:
      return (char *)"P4A : Invalid Binary";
    case CL_INVALID_BUILD_OPTIONS:
      return (char *)"P4A : Invalid Build Options";
    case CL_INVALID_PROGRAM:
      return (char *)"P4A : Invalid Program";
    case CL_INVALID_PROGRAM_EXECUTABLE:
      return (char *)"P4A : Invalid Program Executable";
    case CL_INVALID_KERNEL_NAME:
      return (char *)"P4A : Invalid Kernel Name";
    case CL_INVALID_KERNEL_DEFINITION:
      return (char *)"P4A : Invalid Kernel Definition";
    case CL_INVALID_KERNEL:
      return (char *)"P4A : Invalid Kernel";
    case CL_INVALID_ARG_INDEX:
      return (char *)"P4A : Invalid Arg Index";
    case CL_INVALID_ARG_VALUE:
      return (char *)"P4A : Invalid Arg Value";
    case CL_INVALID_ARG_SIZE:
      return (char *)"P4A : Invalid Arg Size";
    case CL_INVALID_KERNEL_ARGS:
      return (char *)"P4A : Invalid Kernel Args";
    case CL_INVALID_WORK_DIMENSION:
      return (char *)"P4A : Invalid Work Dimension";
    case CL_INVALID_WORK_GROUP_SIZE:
      return (char *)"P4A : Invalid Work Group Size";
    case CL_INVALID_WORK_ITEM_SIZE:
      return (char *)"P4A : Invalid Work Item Size";
    case CL_INVALID_GLOBAL_OFFSET:
      return (char *)"P4A : Invalid Global Offset";
    case CL_INVALID_EVENT_WAIT_LIST:
      return (char *)"P4A : Invalid Event Wait List";
    case CL_INVALID_EVENT:
      return (char *)"P4A : Invalid Event";
    case CL_INVALID_OPERATION:
      return (char *)"P4A : Invalid Operation";
    case CL_INVALID_GL_OBJECT:
      return (char *)"P4A : Invalid GL Object";
    case CL_INVALID_BUFFER_SIZE:
      return (char *)"P4A : Invalid Buffer Size";
    case CL_INVALID_MIP_LEVEL:
      return (char *)"P4A : Invalid Mip Level";
    case CL_INVALID_GLOBAL_WORK_SIZE:
      return (char *)"P4A : Invalid Global Work Size";
    default:
      break;
    }
} 

/** To quit properly OpenCL.
 */
void p4a_clean(int exitCode)
{
  //if(p4a_program)clReleaseProgram(p4a_program);
  if(p4a_kernel)clReleaseKernel(p4a_kernel);  
  if(p4a_queue)clReleaseCommandQueue(p4a_queue);
  if(p4a_context)clReleaseContext(p4a_context);
  exit(exitCode);
}

/* @} */

/** @addtogroup P4A_time_measure

    @{
 */

#if defined(P4A_PROFILING) || defined(P4A_TIMING)
cl_command_queue_properties p4a_queue_properties = CL_QUEUE_PROFILING_ENABLE;
#else
cl_command_queue_properties p4a_queue_properties = 0;
#endif

double p4a_time = 0.;
double p4a_host_time = 0.;
cl_event p4a_event = NULL;
bool timer_call_from_p4a = true;
bool P4A_TIMING_fromHost = false;

/** Counts the number of kernel to display nice timer / kernel load*/
int nKernel = 0;

/** In OpenCL, stop a timer on the accelerator and get float time in
    second.
 */

double P4A_accel_timer_stop_and_float_measure() 
{

#ifdef P4A_PROFILING
  cl_ulong start,end;
  if (timer_call_from_p4a == true) {
    if (!P4A_TIMING_fromHost)    {
      if (p4a_event) {
	clWaitForEvents(1, &p4a_event);
	
	P4A_test_execution(clGetEventProfilingInfo(p4a_event, 
						   CL_PROFILING_COMMAND_END, 
						   sizeof(cl_ulong), 
						   &end, 
						   NULL));
	P4A_test_execution(clGetEventProfilingInfo(p4a_event, 
						   CL_PROFILING_COMMAND_START, 
						   sizeof(cl_ulong), 
						   &start, 
						   NULL));
	// time in nanoseconds	      
	p4a_time += (float)(end - start);
      }
    }
    else {
      // Time in sec
      gettimeofday(&p4a_time_end, NULL);
      double t = (p4a_time_end.tv_sec - p4a_time_begin.tv_sec)
	+ (p4a_time_end.tv_usec - p4a_time_begin.tv_usec)*1e-6;
      // Conversion in nanosec
      p4a_time += t*1.0e9;
      p4a_host_time += t*1.0e9;
      P4A_TIMING_fromHost = false;
    }
  }
  else {
    // p4a_host_time accounts for kernel load
    // This is p4a_time-p4a_host_time that must be compared to cuda kernel timer
    if ((float)p4a_host_time > p4a_time*0.01) {
      fprintf(stderr,"\n*** Begin message from P4A *** \n");

      if (nKernel == 1) 
	fprintf(stderr,"Timer in the host (kernel load), included in the kernel execution time:\n\t %fs\n",
	       p4a_host_time*1.0e-9);
      else 
	fprintf(stderr,"Timer in the host (%d kernels load), included in the kernel execution time:\n\t %fs (%f/s per kernel on average)\n",
	       nKernel,p4a_host_time*1.0e-9,p4a_host_time*1.0e-9/nKernel);

      //fprintf(stderr,"Timer in the accel : %fs\n",(p4a_time-p4a_host_time)*1.0e-9);
      fprintf(stderr,"*** End message from P4A *** \n\n");
    }
  }
  // Return the time in second:
  return p4a_time*1.0e-9;
#else
  return 0;
#endif
}

/* @} */

/** @addtogroup P4A_kernel_call Accelerator kernel call

    @{
*/
cl_context p4a_context = NULL;
cl_command_queue p4a_queue = NULL;
cl_kernel p4a_kernel = NULL;  

#ifdef __cplusplus
std::map<std::string,struct p4a_cl_kernel *> p4a_kernels_map;
#else
//struct p4a_cl_kernel *p4a_kernels=NULL;
// Only used when creating the kernel list
//struct p4a_cl_kernel *last_kernel=NULL;

struct p4a_cl_kernel p4a_kernels_list[MAX_K];


/** @addtogroup P4A_call_parameters Parameters invocation.

    @{
    The C function to invoke clSetKernelArg where the reference to the
    parameter is set as (void *).
*/
void p4a_setArguments(int i,char *s,size_t size,void * ref_arg) 
{
  //fprintf(stderr,"Argument %d : size = %u\n",i,size);
  p4a_global_error = clSetKernelArg(p4a_kernel,i,size,ref_arg);
  P4A_test_execution_with_message("clSetKernelArg");
}
/** 
    @}
*/

/** Creation of a new kernel, initialisation and insertion in the kernel list.
 */
struct p4a_cl_kernel *new_p4a_kernel(const char *kernel)
{
  if (nKernel >= MAX_K) {
    printf("Over max kernel number: %d\n",MAX_K);
    exit(EXIT_FAILURE);
  }
  if (strlen(kernel) >= MAX_CAR-10) {
    printf("Kernel name toolong: %d\n",MAX_CAR);
    exit(EXIT_FAILURE);
  }
    
  strcpy(p4a_kernels_list[nKernel].name,kernel);
  p4a_kernels_list[nKernel].kernel = NULL;
  char* kernelFile;
  asprintf(&kernelFile,"./%s.cl",kernel);
  strcpy(p4a_kernels_list[nKernel].file_name,kernelFile);
}

/** Search if the <kernel> is already in the list.
 */

struct p4a_cl_kernel *p4a_search_current_kernel(const char *kernel)
{
  int i = 0;
  while (i < nKernel && strcmp(p4a_kernels_list[i].name,kernel) != 0) {
    i++;
  }
  if (i < nKernel)
    return &p4a_kernels_list[i];
  else
    return NULL;
}

#endif //!__cplusplus

/** When launching the kernel
    - need to create the program from sources
    - select the kernel call within the program source

    Arguments are pushed from the ... list.
    Two solutions are proposed :
    - One, is actually commented : the parameters list (...)
      must contain the number of parameters, the sizeof(type)
      and the parameter as a reference &;
    - Second, the program source is parsed and the argument list is analysed.
      The result of the aprser is :
      - the number of arguments
      - the sizeof(argument type)

    @param kernel: Name of the kernel and the source MUST have the same
    name with .cl extension.
 */

void p4a_load_kernel(const char *kernel,...)
{
    
#if defined(P4A_PROFILING) || defined(P4A_TIMING)
  P4A_TIMING_fromHost = true;
#endif

#ifdef __cplusplus
  std::string scpy(kernel);
  struct p4a_cl_kernel *k = p4a_kernels_map[scpy];
  
#else
  struct p4a_cl_kernel *k = p4a_search_current_kernel(kernel);
#endif

  // If not found ...
  if (!k) {
    P4A_skip_debug(0,P4A_dump_message("The kernel %s is loaded for the first time\n",kernel));

#ifdef __cplusplus
    k = new p4a_cl_kernel(kernel);
    p4a_kernels_map[scpy] = k;
#else
    new_p4a_kernel(kernel);
    k = &p4a_kernels_list[nKernel];
#endif
    
    nKernel++; 
    char* kernelFile =  k->file_name;
    P4A_skip_debug(0,P4A_dump_message("Program and Kernel creation from %s\n",kernelFile));
    size_t kernelLength=0;
    const char *comment = "// This kernel was generated for P4A\n";
  // Same design as the NVIDIA oclLoadProgSource
   char* cSourceCL = p4a_load_prog_source(kernelFile,
					   comment,
					   &kernelLength);
    if (cSourceCL == NULL)
      P4A_skip_debug(0,P4A_dump_message("source du program null\n"));
    //else
    // P4A_skip_debug(P4A_dump_message("%s\n",cSourceCL));
      
    P4A_skip_debug(0,P4A_dump_message("Kernel length = %lu\n",kernelLength));
    
    /*Create and compile the program : 1 for 1 kernel */
    p4a_program=clCreateProgramWithSource(p4a_context,1,
					  (const char **)&cSourceCL,
					  &kernelLength,
					  &p4a_global_error);
    P4A_test_execution_with_message("clCreateProgramWithSource");
    p4a_global_error=clBuildProgram(p4a_program,0,NULL,NULL,NULL,NULL);
    P4A_test_execution_with_message("clBuildProgram");
    p4a_kernel=clCreateKernel(p4a_program,kernel,&p4a_global_error);
    k->kernel = p4a_kernel;
    P4A_test_execution_with_message("clCreateKernel");
    free(cSourceCL);
  }
  p4a_kernel = k->kernel;
 }

/** @author : Stéphanie Even

    Load and store the content of the kernel file in a string.
    Replace the oclLoadProgSource function of NVIDIA.
 */
char *p4a_load_prog_source(char *cl_kernel_file,const char *head,size_t *length)
{
  // Initialize the size and memory space
  struct stat buf;
  stat(cl_kernel_file,&buf);
  size_t size = buf.st_size;
  size_t len = strlen(head);
  char *source = (char *)malloc(len+size+1);
  strncpy(source,head,len);

  // A string pointer referencing to the position after the head
  // where the storage of the file content must begin
  char *p = source+len;

  // Open the file
  int in = open(cl_kernel_file,O_RDONLY);
  if (in==-1) {
    fprintf(stderr,"Bad kernel source reference : %s\n",cl_kernel_file);
    exit(EXIT_FAILURE);
  }
  
  // Read the file content
  int n=0;
  if ((n = read(in,(void *)p,size)) != (int)size) {
    fprintf(stderr,"Read was not completed : %d / %lu octets\n",n,size);
    exit(EXIT_FAILURE);
  }
  
  // Final string marker
  source[len+n]='\0';
  close(in);
  *length = size+len;
  return source;
}

/** @} */

/** @defgroup P4A_memory_allocation_copy Memory allocation and copy 

    @{
*/
/** Allocate memory on the hardware accelerator in OpenCL mode.

    @param[out] address is the address of a variable that is updated by
    this function to contains the address of the allocated memory block

    @param[in] size is the size to allocate in bytes
*/
void P4A_accel_malloc(void **address, size_t size) {
  // The mem flag can be : 
  // CL_MEM_READ_ONLY, 
  // CL_MEM_WRITE_ONLY, 
  // CL_MEM_READ_WRITE, 
  // CL_MEM_USE_HOST_PTR
  *address=(void *)clCreateBuffer(p4a_context,
				  CL_MEM_READ_WRITE,
				  size,
				  NULL,
				  &p4a_global_error);
  P4A_test_execution_with_message("Memory allocation via clCreateBuffer");
}

/** Free memory on the hardware accelerator in OpenCL mode

    @param[in] address points to a previously allocated memory zone for
    the hardware accelerator
*/
void P4A_accel_free(void *address) 
{
  p4a_global_error=clReleaseMemObject((cl_mem)address);
  P4A_test_execution_with_message("clReleaseMemObject");
}

/** Copy a scalar from the hardware accelerator to the host

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_from_accel(size_t element_size,
			 void *host_address,
			 const void *accel_address) 
{
#ifdef P4A_TIMING
  P4A_TIMING_accel_timer_start;
#endif

  p4a_global_error=clEnqueueReadBuffer(p4a_queue,
				       (cl_mem)accel_address,
				       CL_TRUE, // synchronous read
				       0,
				       element_size,
				       host_address,
				       0,
				       NULL,
				       &p4a_event);
  timer_call_from_p4a = true;
  P4A_accel_timer_stop_and_float_measure();
  timer_call_from_p4a = false;
  P4A_test_execution_with_message("clEnqueueReadBuffer");

#ifdef P4A_TIMING
  P4A_TIMING_accel_timer_stop;
  P4A_TIMING_elapsed_time(p4a_timing_elapsedTime);
  P4A_dump_message("Copied %zd bytes of memory from accel %p to host %p : "
		   "%.1fms - %.2fGB/s\n",
		   element_size, accel_address,host_address,
		   p4a_timing_elapsedTime,
		   (float)element_size/(p4a_timing_elapsedTime*1000000));
#endif
}

/** Copy a scalar from the host to the hardware accelerator

  @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_to_accel(size_t element_size,
		       const void *host_address,
		       void *accel_address) 
{
#ifdef P4A_TIMING
  P4A_TIMING_accel_timer_start;
#endif

  p4a_global_error=clEnqueueWriteBuffer(p4a_queue,
					(cl_mem)accel_address,
					CL_FALSE,// asynchronous write
					0,
					element_size,
					host_address,
					0,
					NULL,
					&p4a_event);
  timer_call_from_p4a = true;
  P4A_accel_timer_stop_and_float_measure();
  timer_call_from_p4a = false;
  P4A_test_execution_with_message("clEnqueueWriteBuffer");

#ifdef P4A_TIMING
  P4A_TIMING_accel_timer_stop;
  P4A_TIMING_elapsed_time(p4a_timing_elapsedTime);
  P4A_dump_message("Copied %zd bytes of memory from host %p to accel %p : "
		   "%.1fms - %.2fGB/s\n",element_size, host_address,accel_address,
		   p4a_timing_elapsedTime,
		   (float)element_size/(p4a_timing_elapsedTime*1000000));
#endif
}

/** Function for copying memory from the hardware accelerator to a 1D array in
 the host.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from (host side)

 @param[out] host_address point to the array on the host to write into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.

 @return the host_address, by compatibility with memcpy().
 */
void P4A_copy_from_accel_1d(size_t element_size,
			    size_t d1_size,
			    size_t d1_block_size,
			    size_t d1_offset,
			    void *host_address,
			    const void *accel_address) 
{
  char * cdest = d1_offset*element_size + (char *)host_address;
  // CL_TRUE : synchronous read
  p4a_global_error=clEnqueueReadBuffer(p4a_queue,
				       (cl_mem)accel_address,
				       CL_TRUE,
				       0,
				       d1_block_size*element_size,
				       cdest,
				       0,
				       NULL,
				       &p4a_event);
  P4A_accel_timer_stop_and_float_measure();
  P4A_test_execution_with_message("clEnqueueReadBuffer");
}


/** Function for copying a 1D memory zone from the host to a compact memory
 zone in the hardware accelerator.

 This function could be quite simpler but is designed by symmetry with
 other functions.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from

 @param[in] host_address point to the array on the host to read

 @param[out] accel_address refer to the compact memory area to write
 data. In the general case, accel_address may be seen as a unique idea
 (FIFO) and not some address in some memory space.
 */
void P4A_copy_to_accel_1d(size_t element_size,
			  size_t d1_size,
			  size_t d1_block_size,
			  size_t d1_offset,
			  const void *host_address,
			  void *accel_address) 
{
  const char * csrc = d1_offset*element_size + (char *)host_address;
  p4a_global_error=clEnqueueWriteBuffer(p4a_queue,
					(cl_mem)accel_address,
					CL_FALSE, //CL_FALSE:asynchronous write
					0,
					d1_block_size*element_size,
					csrc,
					0,
					NULL,
					&p4a_event);
  P4A_accel_timer_stop_and_float_measure();
  P4A_test_execution_with_message("clEnqueueWriteBuffer");
}

/** Function for copying memory from the hardware accelerator to a 2D array in
 the host.
 */
/*void P4A_copy_from_accel_2d(size_t element_size,
			    size_t d1_size, size_t d2_size,
			    size_t d1_block_size, size_t d2_block_size,
			    size_t d1_offset, size_t d2_offset,
			    void *host_address,
			    const void *accel_address) 
{
  // d2 is fully transfered ? We can optimize :-)
  if(d2_size==d2_block_size ) { 
    // We transfer all in one shot !
    P4A_copy_from_accel_1d(element_size,
                           d2_size * d1_size,
                           d2_size * d1_block_size,
                           d1_offset*d2_size+d2_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_1d(element_size,
                             d2_size,
                             d2_block_size,
                             d2_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }
}
*/
/** Function for copying a 2D memory zone from the host to a compact memory
 zone in the hardware accelerator.
 */
/*void P4A_copy_to_accel_2d(size_t element_size,
			  size_t d1_size, size_t d2_size,
			  size_t d1_block_size, size_t d2_block_size,
			  size_t d1_offset, size_t d2_offset,
			  const void *host_address,
			  void *accel_address) 
{
  // d2 is fully transfered ? We can optimize :-)
  if(d2_size==d2_block_size ) { 
    // We transfer all in one shot !
    P4A_copy_to_accel_1d(element_size,
                         d2_size * d1_size,
                         d2_size * d1_block_size,
                         d1_offset*d2_size+d2_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;
    
    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_1d(element_size,
                           d2_size,
                           d2_block_size,
                           d2_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }

}
*/
/** Function for copying memory from the hardware accelerator to a 3D array in
    the host.
*/
/*
void P4A_copy_from_accel_3d(size_t element_size,
			    size_t d1_size, size_t d2_size, size_t d3_size,
			    size_t d1_block_size, 
			    size_t d2_block_size, size_t d3_block_size,
			    size_t d1_offset, 
			    size_t d2_offset, size_t d3_offset,
			    void *host_address,
			    const void *accel_address) 
{
  // d3 is fully transfered ? We can optimize :-)
  if(d3_size==d3_block_size ) { 
    // We transfer all in one shot !
    P4A_copy_from_accel_2d(element_size,
                           d1_size,d3_size * d2_size,
                           d1_block_size, d3_size * d2_block_size,
                           d1_offset, d2_offset*d3_size+d3_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_2d(element_size,
                             d2_size,d3_size,
                             d2_block_size, d3_block_size,
                             d2_offset, d3_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}

*/
/** Function for copying a 3D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
/*
void P4A_copy_to_accel_3d(size_t element_size,
			  size_t d1_size, size_t d2_size, size_t d3_size,
			  size_t d1_block_size, 
			  size_t d2_block_size, 
			  size_t d3_block_size,
			  size_t d1_offset, size_t d2_offset, size_t d3_offset,
			  const void *host_address,
			  void *accel_address) 
{
  // d3 is fully transfered ? We can optimize :-)
  if(d3_size==d3_block_size ) { 
    // We transfer all in one shot !
    P4A_copy_to_accel_2d(element_size,
                         d1_size,d3_size * d2_size,
                         d1_block_size, d3_size * d2_block_size,
                         d1_offset, d2_offset*d3_size+d3_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_2d(element_size,
                           d2_size,d3_size,
                           d2_block_size, d3_block_size,
                           d2_offset, d3_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}
*/
/** Function for copying memory from the hardware accelerator to a 4D array in
    the host.
*/
/*
void P4A_copy_from_accel_4d(size_t element_size,
			    size_t d1_size, size_t d2_size, 
			    size_t d3_size, size_t d4_size,
			    size_t d1_block_size, size_t d2_block_size, 
			    size_t d3_block_size, size_t d4_block_size,
			    size_t d1_offset, size_t d2_offset, 
			    size_t d3_offset, size_t d4_offset,
			    void *host_address,
			    const void *accel_address) 
{
  // d4 is fully transfered ? We can optimize :-)
  if(d4_size==d4_block_size ) { 
    // We transfer all in one shot !
    P4A_copy_from_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}
*/
/** Function for copying a 4D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
/*
void P4A_copy_to_accel_4d(size_t element_size,
			  size_t d1_size, size_t d2_size, 
			  size_t d3_size, size_t d4_size,
			  size_t d1_block_size, size_t d2_block_size, 
			  size_t d3_block_size, size_t d4_block_size,
			  size_t d1_offset, size_t d2_offset, 
			  size_t d3_offset, size_t d4_offset,
			  const void *host_address,
			  void *accel_address) 
{
  //d4 is fully transfered ? We can optimize :-)
  if (d4_size == d4_block_size) { 
    // We transfer all in one shot !
    P4A_copy_to_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } 
  else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}
*/
/* @} */





#endif // P4A_ACCEL_OPENCL

#ifndef P4A_ACCEL_OPENCL
/** Function for copying memory from the hardware accelerator to a 1D array in
 the host.

 It's a wrapper around CudaMemCopy*.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from (host side)

 @param[out] host_address point to the array on the host to write into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.

 @return the host_address, by compatibility with memcpy().
 */
void P4A_copy_from_accel_1d(size_t element_size,
    size_t d1_size,
    size_t d1_block_size,
    size_t d1_offset,
    void *host_address,
    const void *accel_address) {
  const char * cdest = d1_offset*element_size + (char *)host_address;
  P4A_copy_from_accel(d1_block_size * element_size, (void *)cdest, accel_address);
}

/** Function for copying a 1D memory zone from the host to a compact memory
 zone in the hardware accelerator.

 This function could be quite simpler but is designed by symmetry with
 other functions.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from

 @param[in] host_address point to the array on the host to read

 @param[out] accel_address refer to the compact memory area to write
 data. In the general case, accel_address may be seen as a unique idea
 (FIFO) and not some address in some memory space.
 */
void P4A_copy_to_accel_1d(size_t element_size,
    size_t d1_size,
    size_t d1_block_size,
    size_t d1_offset,
    const void *host_address,
    void *accel_address) {
  const char * csrc = d1_offset*element_size + (char *)host_address;
  P4A_copy_to_accel(d1_block_size * element_size, csrc, accel_address);
}

#endif // P4A_ACCEL_OPENCL

/** Function for copying memory from the hardware accelerator to a 2D array in
 the host.
 */
void P4A_copy_from_accel_2d(size_t element_size,
    size_t d1_size, size_t d2_size,
    size_t d1_block_size, size_t d2_block_size,
    size_t d1_offset, size_t d2_offset,
    void *host_address,
    const void *accel_address) {

  if(d2_size==d2_block_size ) { // d2 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_1d(element_size,
                           d2_size * d1_size,
                           d2_size * d1_block_size,
                           d1_offset*d2_size+d2_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_1d(element_size,
                             d2_size,
                             d2_block_size,
                             d2_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }
}

/** Function for copying a 2D memory zone from the host to a compact memory
 zone in the hardware accelerator.
 */
void P4A_copy_to_accel_2d(size_t element_size,
    size_t d1_size, size_t d2_size,
    size_t d1_block_size, size_t d2_block_size,
    size_t d1_offset, size_t d2_offset,
    const void *host_address,
    void *accel_address) {
  if(d2_size==d2_block_size ) { // d2 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_1d(element_size,
                         d2_size * d1_size,
                         d2_size * d1_block_size,
                         d1_offset*d2_size+d2_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_1d(element_size,
                           d2_size,
                           d2_block_size,
                           d2_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }

}

/** Function for copying memory from the hardware accelerator to a 3D array in
    the host.
*/
void P4A_copy_from_accel_3d(size_t element_size,
          size_t d1_size, size_t d2_size, size_t d3_size,
          size_t d1_block_size, size_t d2_block_size, size_t d3_block_size,
          size_t d1_offset, size_t d2_offset, size_t d3_offset,
          void *host_address,
          const void *accel_address) {


  if(d3_size==d3_block_size ) { // d3 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_2d(element_size,
                           d1_size,d3_size * d2_size,
                           d1_block_size, d3_size * d2_block_size,
                           d1_offset, d2_offset*d3_size+d3_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_2d(element_size,
                             d2_size,d3_size,
                             d2_block_size, d3_block_size,
                             d2_offset, d3_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}


/** Function for copying a 3D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
void P4A_copy_to_accel_3d(size_t element_size,
        size_t d1_size, size_t d2_size, size_t d3_size,
        size_t d1_block_size, size_t d2_block_size, size_t d3_block_size,
        size_t d1_offset,   size_t d2_offset, size_t d3_offset,
        const void *host_address,
        void *accel_address) {

  if(d3_size==d3_block_size ) { // d3 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_2d(element_size,
                         d1_size,d3_size * d2_size,
                         d1_block_size, d3_size * d2_block_size,
                         d1_offset, d2_offset*d3_size+d3_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_2d(element_size,
                           d2_size,d3_size,
                           d2_block_size, d3_block_size,
                           d2_offset, d3_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}

/** Function for copying memory from the hardware accelerator to a 4D array in
    the host.
*/
void P4A_copy_from_accel_4d(size_t element_size,
          size_t d1_size, size_t d2_size, size_t d3_size, size_t d4_size,
          size_t d1_block_size, size_t d2_block_size, size_t d3_block_size, size_t d4_block_size,
          size_t d1_offset, size_t d2_offset, size_t d3_offset, size_t d4_offset,
          void *host_address,
          const void *accel_address) {

  if(d4_size==d4_block_size ) { // d4 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}

/** Function for copying a 4D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
void P4A_copy_to_accel_4d(size_t element_size,
          size_t d1_size, size_t d2_size, size_t d3_size, size_t d4_size,
          size_t d1_block_size, size_t d2_block_size, size_t d3_block_size, size_t d4_block_size,
          size_t d1_offset, size_t d2_offset, size_t d3_offset, size_t d4_offset,
          const void *host_address,
          void *accel_address) {
  if(d4_size==d4_block_size ) { // d4 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}
