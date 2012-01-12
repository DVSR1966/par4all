If you do not have write access to this directory, copy its content (with
cp -a for example) somewhere you can write into and try the examples from
this new location.

This directory contains par4all basic examples : Hyantes, Jacobi, Stars-pm
and saxpy_c99.

To run one example, go into one of the directories.

Then you can parallelize, build and execute that example using the
following commands. These commands are generic, and more specific commands
can be found in the directory of each example.

You can directly make a "run_..." target or even a "display_..." target if
that makes sense, from the following.

For the sequential execution

  make seq : build the sequential program

  make run_seq : build first if needed, then run the sequential program

  make display_seq : build first if needed, then run the sequential
  program and display results

For the OpenMP parallel execution on multi-cores:

  make openmp : parallelize the code to OpenMP source and compile

  make run_openmp : build first if needed, then run the OpenMP parallel
  program

  make display_openmp : build first if needed, then run the OpenMP
  parallel program and display results

For the CUDA parallel execution on nVidia GPU:

  make cuda : parallelize the code to CUDA source and compile

  make run_cuda : build first if needed, then run the CUDA parallel
  program

  make display_cuda : build first if needed, then run the CUDA parallel
  program and display results

    Do not forget to have the CUDA runtime correctly
    installed. LD_LIBRARY_PATH should contain at least the location of
    CUDA runtime library.

For the CUDA optimized and parallel execution on nVidia GPU:

  make cuda-opt : parallelize the code to CUDA source and compile

  make run_cuda-opt : build first if needed, then run the CUDA parallel
  program

  make display_cuda-opt : build first if needed, then run the CUDA
  parallel program and display results

For an OpenMP parallel emulation of a GPU-like accelerator (useful for
debugging, without any GPU):

  make accel-openmp : parallelize the code to GPU-like OpenMP source and compile

  make run_accel-openmp : build first if needed, then run the parallel program

  make display_accel-openmp : build first if needed, then run the parallel
  program and display results

For the OpenCL parallel execution on nVidia GPU:

  make opencl : parallelize the code to OpenCL sources: the host program sources
  *.p4a.c and the kernel program sources *.cl

  make run_opencl : build first if needed, then run the OpenCL parallel
  program

  make display_opencl : build first if needed, then run the OpenCL
  parallel program and display results

	To compile for nVidia GPU, you need CUDA environment installed.

To chain all the demos:

  make demo

You might also run "make clean" first to force rebuilding.


Makefile tweaking:
==================

You can set the P4A_OPTIONS variable to pass some options to p4a.

For example if you have an old CUDA 1.3 GPU, you can run the demo with:

export P4A_OPTIONS='--cuda-cc=1.3'
make demo

or

make P4A_OPTIONS='--cuda-cc=1.3' demo

If you have a GPU that cannot cope with double precision floating point
computation, try:
make USE_FLOAT=1 P4A_OPTIONS='--cuda-cc=1.1' ...


You can use NVCCFLAGS to give options to the nvcc CUDA compiler:

  NVCCFLAGS="-arch sm_11" make run_cuda-manual




### Local Variables:
### mode: flyspell
### ispell-local-dictionary: "american"
### End:
