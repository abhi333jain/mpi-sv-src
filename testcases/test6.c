/* -*- Mode: C; -*- */
/* Creator: Bronis R. de Supinski (bronis@llnl.gov) Tue Aug 26 2003 */
/* any_src-can-deadlock4.c -- deadlock occurs if task 0 receives */
/*                            from task 1 first; pretty random */
/*                            as to which is received first... */

#include <stdio.h>
#include "mpi.h"

#define buf_size 128

int
main (int argc, char **argv)
{
  int nprocs = -1;
  int rank = -1;
  char processor_name[128];
  int namelen = 128;
  int buf0[buf_size];
  int buf1[buf_size];
  MPI_Status status;
  MPI_Request req;
MPI_Request req2;
  /* init */
  MPI_Init (&argc, &argv);
  MPI_Comm_size (MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Get_processor_name (processor_name, &namelen);
  printf ("(%d) is alive on %s\n", rank, processor_name);
  fflush (stdout);

  MPI_Barrier (MPI_COMM_WORLD);

  if (nprocs != 3)
    {
      printf ("error tasks\n");
    }
  else if (rank == 0)
    {  
       memset (buf1, 1, buf_size);
      MPI_Irecv (buf1, buf_size, MPI_INT, 
         1, 0, MPI_COMM_WORLD, &req2);
      MPI_Irecv (buf1, buf_size, MPI_INT, 
         MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &req);
      MPI_Wait (&req, &status);
    }
  else if(rank==1)
    {
      memset (buf0, 0, buf_size);
      MPI_Send (buf0, buf_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
      MPI_Send (buf0, buf_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
   else{
      memset (buf0, 0, buf_size);
      MPI_Send (buf0, buf_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
   }
  MPI_Barrier (MPI_COMM_WORLD);

  MPI_Finalize ();
  printf ("(%d) Finished normally\n", rank);
}

/* EOF */
