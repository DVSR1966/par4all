#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "stars-pm.h"

void printUsage(int argc, char **argv) {
  printf("Usage : \n\n%s (-f||--file) file\n", argv[0]);
}

int main(int argc, char **argv) {

  /******************************************************
   *                    ALLOCATIONS
   ******************************************************/

  float mp[NP][NP][NP]; // Mass for each particle
  coord pos[NP][NP][NP]; // Position (x,y,z) for each particle
  coord vel[NP][NP][NP]; // Velocity (x,y,z) for each particle

  // These are temporary
  float dens[NP][NP][NP]; // Density for each particle
  float force[NP][NP][NP]; // Force for each particle

  int data[NP][NP][NP];
  int histo[NP][NP][NP];

  //  fftwf_complex *cdens; // Density, complex representation for FFT
  float cdens[NP][NP][NP][2];

  float time;
  int npdt = 0;
  float dt = DT / 2; // Will be 1/2 only for the first iteration
  char * icfile;
  double start_time,end_time; // Timing stuff


  /******************************************************
   *               GET INITIAL CONDITIONS
   ******************************************************/
  icfile = argv[1];
  if(NULL == icfile) {
    printUsage(argc, argv);
    exit(-1);
  }

  // Read input file
  readDataFile(mp, pos, vel, icfile);

  potential_init_plan(cdens); // Init fft stuff

#ifdef _GLGRAPHICS_
      graphic_gldraw(argc, argv, pos); // Initialize Opengl
#endif

  /*** TIMING ***/
  start_time = get_time();
  /***        ***/

  // Force the copy-in here so that we are timing it !
  if(argc==0)
    readDataFile(mp, pos, vel, icfile);


  /******************************************************
   *                  MAIN LOOP !!
   ******************************************************/
  for (time = 0; time <= TMAX; time += DT) {
#ifndef P4A_BENCH
    if(0 == npdt % MODDISP) {
      puts("**********************************");
      printf("Time= %5.2e Npdt= %d\n", time, npdt);
    }
#endif
    // Stage 1 : discretization of particles position
    discretization(pos, data);

    // Stage 2 : computing density, sequential histogramming pass
    histogram(data, histo);


#ifdef _GRAPHICS_
    if(0 == npdt % MODDISP) {
      // GTK graphic output
      graphic_draw(argc, argv, histo);
    }
#endif

    // Stage 3 : computing potential from density
    potential(histo, dens, cdens, mp);

    // Stage 4 : computing force from potential in each direction, and apply it on velocity
    forcex(dens, force);
    updatevel(vel, force, data, 0, dt);

    forcey(dens, force);
    updatevel(vel, force, data, 1, dt);

    forcez(dens, force);
    updatevel(vel, force, data, 2, dt);

    // Final stage : updating position using new velocity
    updatepos(pos, vel);


    // Timestep is 1/2 DT for first iteration
    dt = DT;


#ifdef _GLGRAPHICS_
    graphic_glupdate(pos);
#endif
#ifdef _DUMPPOS_
      dump_pos(pos,npdt);
#endif

    npdt++;
  }
  //************************************  END LOOP  ********************************

  // Force synchro when using async launch on the GPU
  // Force a copy-out here so that we are timing it !
  if(argc==0)
    dump_pos(pos,npdt);


  /*** TIMING ***/
  end_time = get_time();
  fprintf(stderr," P4A: Time for '%s' : %fms\n",__FUNCTION__, (end_time-start_time)*1000);
  /***        ***/


  // Free ftt allocation
  potential_free_plan();

#ifdef _GRAPHICS_
  graphic_destroy(); // Free GTK stuff
#endif
#ifdef _GLGRAPHICS_
  graphic_gldestroy(); // Free opengl stuff
#endif


#ifndef P4A_BENCH
  puts("-----------------------------");
  puts("         Finished");
  puts("-----------------------------");
#endif

  return 0;
}
