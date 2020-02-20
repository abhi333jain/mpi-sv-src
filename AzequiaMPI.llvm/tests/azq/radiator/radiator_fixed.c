/*----------------------------------------------------------------*
 *   Declaration of public types and functions                    *
 *   used by this module                                          *
 *----------------------------------------------------------------*/
#ifdef __OSI
#include <osi.h>
#endif
#include <azq.h>
#include <azq_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>


/*----------------------------------------------------------------*
 *   Definition of private constants                              *
 *----------------------------------------------------------------*/
#define  PRINT_PERIOD  10
#define  ITERA         10000

#define  MARK          0xAA
/*----------------------------------------------------------------*
 *   Implementation of private functions                          *
 *----------------------------------------------------------------*/
double abstime (void) {

  struct timespec t;

  clock_gettime(CLOCK_REALTIME, &t);

  return ((double) t.tv_sec + 1.0e-9 * (double) t.tv_nsec);
}


int do_sender (int bufSize, int senders, int receivers) {

  int      myid;
  int      gix;
  int     *value;
  int      acum;
  Rqst_t   rqst_ptr;
  Rqst_t   rqst_ptent;
  Rqst     rqst;
  Rqst     rqst_per;
  Status   status;
  int      itr  = 0;
  int      i;
  Addr     dst;
  int      excpn;
  double   now_1;
  double   now_0;
  int      cancelled = 0;
  int      now_cancel;


  myid = getRank();
  gix  = getGroup();
#ifdef __DEBUG
  fprintf(stdout, "Sender(%x). [%d, %d]\n", (unsigned int)(THR_self()), getGroup(), myid);
#endif

  now_0 = abstime();

  /* Get the memory for the data to be sent */
  if(NULL == (value = (int *)malloc(bufSize))) {
    fprintf(stdout, "::::::::::::::::: Bug Node: Malloc Exception %d ::::::::::::::::\n", -5);
    return(-5);
  }

  for(i = 0; i < bufSize / sizeof(int); i++) {
    value[i] = i;
    acum += i;
  }

  /* Configure destination address */
  dst.Group = getGroup();
  dst.Rank  = senders + receivers;

  /* Persistent requests */
  rqst_ptent = &rqst_per;
  psend_init(&dst, (char *)&value[0], bufSize, 0, rqst_ptent);


  while (1) {

    for(i = 0; i < bufSize / sizeof(int); i++)
      value[i] = myid;

#ifdef __DEBUG
    if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t= S %d ============== itr [%d] [size: %d] [cancelled: %d] ...\n", myid, itr, bufSize, cancelled);
#endif

    switch((rand() % 4) + 0) {

      case 0: /* SYNCHRONOUS SEND */
        
        if(0 > (excpn = send(&dst, (char *)(&value[0]), bufSize, 0))) {
          fprintf(stderr, "\n:: S :::::::::: Sender(%x). [%d  %d]: Exception %d -SEND-::::::::::\n", (int)(THR_self()), gix, myid, excpn);
          free(value);
          return(excpn);
        }
        break;

      case 1: /* ASYNCHRONOUS SEND */

        rqst_ptr = &rqst;
        if(0 > (excpn = asend(&dst, (char *)(&value[0]), bufSize, 0, rqst_ptr))) {
          fprintf(stderr, "\n:: S :::::::::: Sender(%x). [%d  %d]: Exception %d -ASEND-:::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          free(value);
          return(excpn);
        }

        if(0 > (excpn = wait(&rqst_ptr, AZQ_STATUS_IGNORE)))  {
          fprintf(stderr, "\n:: S :::::::::: Sender(%x). [%d  %d]: Exception %d -ASEND/WAIT-:::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          free(value);
          return(excpn);
        }

        break;

      case 2: /* ASYNCHRONOUS SEND WITH CANCEL */

        do {

          rqst_ptr = &rqst;
          if(0 > (excpn = asend(&dst, (char *)&value[0], bufSize, 0, rqst_ptr))) {
            fprintf(stderr, "\n:: S :::::::::: Sender(%x). [%d  %d]: Exception %d -SEND/CANCEL- :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            free(value);
            return(excpn);
          }

          //if (status.Cancelled) usleep(100);
          //sched_yield();
          status.Cancelled = 0;
          cancel(rqst_ptr);

          if(0 > (excpn = wait(&rqst_ptr, &status)))  {
            fprintf(stderr, "\n:: S :::::::::: Sender(%x). [%d  %d]: Exception %d -SEND/CANCEL/WAIT- :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            free(value);
            return(excpn);
          }

          if   (status.Cancelled)  { cancelled++;  }
          else                     {  }                     

        } while (status.Cancelled);
        break;

      case 3: /* PERSISTENT SEND */

        if(0 > (excpn = psend_start(rqst_ptent))) {
          fprintf(stderr, "\n:: S :::::::::: Sender(%x). [%d  %d]: Exception %d -PSEND- :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          free(value);
          return(excpn);
        }

        if(0 > (excpn = wait(&rqst_ptent, AZQ_STATUS_IGNORE)))  {
          fprintf(stderr, "\n:: S :::::::::: Sender(%x). [%d  %d]: Exception %d -PSEND/WAIT- :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          free(value);
          return(excpn);
        }
        break;
        
    }

    if (++itr == ITERA)
      break;
  }

  if(value)
    free(value);

  now_1 = abstime();
#ifdef __DEBUG
  fprintf(stdout, "\nmilliseconds %lf\n", (now_1 - now_0));
  fprintf(stdout, "%lf us/msg \n", 1000 * (((double)(now_1 - now_0)) / (double)ITERA));

  fprintf(stdout, "Sender(%x). [%d, %d].  BYE\n", (unsigned int)(THR_self()), getGroup(), myid);
#endif

  return 0;
}



int do_receiver (int bufSize, int senders, int receivers) {
    
  Rqst       rqst;
  Rqst       rqst_per;
  Rqst_t     rqst_ptr;
  Rqst_t     rqst_ptent;
  Status     status;
  Status     status_prob;
  int        myid;
  int        gix;
  int        itr = 0;
  double     now_1;
  double     now_0;
  int        rank;
  Addr       src;
  int        excpn;
  int        flag;
  int        cancelled = 0;
  int        aprobed = 0;
  int        now_cancel;
  int        probe_tag;
  int        probe_cnt;


  myid = getRank();
  gix  = getGroup();
#ifdef __DEBUG
  fprintf(stdout, "Receiver(%x). [%d, %d]\n", (unsigned int)(THR_self()), getGroup(), myid);
#endif

  now_0 = abstime();

  /* Configure source address */
  src.Group = getGroup();
  src.Rank  = senders + receivers;

  /* Persistent requests */
  rqst_ptent = &rqst_per;
  precv_init(&src, (char *)&rank, sizeof(int), 9, rqst_ptent);

  while (1) {

#ifdef __DEBUG
    if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t= R %d ============== itr [%d] [cancelled: %d] [aprobed: %d] ...\n", myid, itr, cancelled, aprobed);
#endif

    switch((rand() % 6) + 0) { 

      case 0:  /* SYNCHRONOUS RECEIVE */

        if(0 > (excpn = recv(&src, (char *)&rank, sizeof(int), 9, &status))) {
          fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }
        break;
      
      case 1:  /* ASYNCHRONOUS RECEIVE */

        rqst_ptr = &rqst;
        if(0 > (excpn = arecv(&src, (char *)&rank, sizeof(int), 9, rqst_ptr))) {
          fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d in arecv :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }
        if(0 > (excpn = wait(&rqst_ptr, &status)))  {
          fprintf(stderr, "\n:: R :::::::::: Radiator(%x). [%d  %d]: Exception %d  in wait_recv :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }
        break;
      
      case 2:  /* ASYNCHRONOUS RECEIVE + CANCEL */

        do {
          rqst_ptr = &rqst;
          if(0 > (excpn = arecv(&src, (char *)&rank, sizeof(int), 9, rqst_ptr))) {
            fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d in arecv :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            return(excpn);
          }
       
          status.Cancelled = 0;
          cancel(rqst_ptr);
          
          if(0 > (excpn = wait(&rqst_ptr, &status)))  {
            fprintf(stderr, "\n:: R :::::::::: Radiator(%x). [%d  %d]: Exception %d  in wait_recv :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            return(excpn);
          }

          if   (status.Cancelled)  { /*fprintf(stdout, "*"); */ cancelled++; /*usleep(10); sched_yield();*/ }
          else                     { /*fprintf(stdout, "/"); */ }

        } while (status.Cancelled);

        break;

      case 3: /* PERSISTENT RECEIVE */
         
        if(0 > (excpn = precv_start(rqst_ptent))) {
          fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d in precv :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }

        if(0 > (excpn = wait(&rqst_ptent, &status)))  {
          fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d in precv-wait :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }
        break;

      case 4:  /* ASYNCHRONOUS PROBE - SLOW call, deactivated by default */

        src.Rank = ADDR_RNK_ANY;
        
        flag = FALSE;
        while (!flag) {
          if(0 > (excpn = aprobe (&src, TAG_ANY, &flag, &status))) {
            fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            return(excpn);
          }
          if (!flag)  { /*fprintf(stdout, "=");*/ aprobed++; /*sched_yield();*/ }
        }
 
        src.Group = status.Src.Group;
        src.Rank  = status.Src.Rank;
        if(0 > (excpn = recv (&src, (char *)&rank, status.Count, status.Tag, &status_prob))) {
          fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }
        break;

      case 5:  /* PROBE */

        src.Rank = ADDR_RNK_ANY;
        if(0 > (excpn = probe (&src, TAG_ANY, &status))) {
          fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }

        if(0 > (excpn = recv (&src, (char *)&rank, status.Count, status.Tag, &status_prob))) {
          fprintf(stderr, "\n:: R :::::::::: Receiver(%x). [%d  %d]: Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
          return(excpn);
        }
        break;

    }

    if(myid != rank) {
      fprintf(stderr, "= R ============== Receiver(%x):  Bad range %d\n", (unsigned int)(THR_self()), rank);
      return(-1);
    }

    if (++itr == ITERA)
      break;
    
  }
  now_1 = abstime();
#ifdef __DEBUG
  fprintf(stdout, "\nmilliseconds %lf\n", (now_1 - now_0));
  fprintf(stdout, "%lf us/msg \n", 1000 * (((double)(now_1 - now_0)) / (double)ITERA));

  fprintf(stdout, "Receiver(%x). [%d, %d].  BYE\n", (unsigned int)(THR_self()), getGroup(), myid);
#endif

  return(0);
}



int do_radiator (int bufSize, int senders, int receivers) {

  int       myid;
  int       gix;
  Rqst     *rqst;
  Rqst_t    all_rqst[senders + receivers + 2];
  Rqst_t    rqst_ptr;
  Status    status[senders + receivers + 2];
  Status    status_any;
  int       idx;
  int       itr = 0;
  int       i, j, k;
  int      *s[senders];
  int       index[senders + receivers + 2];
  int       rank[receivers];
  Addr      src, dst;
  int       excpn;
  int       outCnt;
  int       doneCnt;
  int       sum;
  int       acum;
  int       flag;
  int       aleat;

  myid = getRank();
  gix  = getGroup();
#ifdef __DEBUG
  fprintf(stdout, "Radiator(%x). [%d, %d]\n", (int)(THR_self()), getGroup(), myid);
#endif

  for(j = 0; j < senders; j++) {
    if(NULL == (s[j] = (int *)malloc(bufSize)))   {
      fprintf(stderr, " :::::::::: Radiator(%x) (R): Malloc Exception %d :::::::::: free ... ", (int)THR_self(), -5);
      for(i = 0; i < j; i++)
        free(s[i]);
      return(-5);
    }
    s[j][0] = 2;
  }

  if (NULL == (rqst = (Rqst_t) malloc (sizeof(Rqst) * (senders + receivers)))) {
    fprintf(stderr, " :::::::::: Radiator(%x) (R): Malloc Exception %d :::::::::: free ... ", (int)THR_self(), -6);
    for(i = 0; i < senders; i++)
      free(s[i]);
    return(-6);
  }

  while(1) {

#ifdef __DEBUG
    if(itr % PRINT_PERIOD == 0)     fprintf(stdout, "\t\t= CR ============== itr [%d]...\n", itr);
#endif

    for(k = 0; k < senders + receivers; k++) {
      all_rqst[k] = &rqst[k];
    }
    all_rqst[k]     = NULL;
    all_rqst[k + 1] = NULL;

    /* ASYNCHRONOUS RECEIVE FROM ALL SENDERS */
    for(j = 0; j < senders; j++) {

      src.Group = getGroup();
      src.Rank  = j;

      if(0 > (excpn = arecv (&src, (char *)s[j], bufSize, 0, all_rqst[j])))  {
        fprintf(stderr, "\n :::::::::: Radiator-R(%x). [%d  %d]: Exception %d :::::::::: \n", (unsigned int)(THR_self()), gix, myid, excpn);
        for(i = 0; i < senders; i++)
          free(s[i]);
        return(excpn);
      }

    }

    /* ASYNCHRONOUS SEND TO ALL RECEIVERS */
    for(j = 0; j < receivers; j++) {

      dst.Group = getGroup();
      dst.Rank  = senders + j;
      rank[j]   = senders + j;

      if(0 > (excpn = asend (&dst, (char *)&rank[j], sizeof(int), 9, all_rqst[senders + j])))  {
        fprintf(stderr, "\n :::::::::: Radiator(%x). [%d  %d]: Exception %d :::::::::: \n", (unsigned int)(THR_self()), gix, myid, excpn);
        for(i = 0; i < senders; i++)
          free(s[i]);
        return(excpn);
      }

    }

    /* COMPLETE COMMUNICATION */

    do {    
      aleat = rand() % 8;
    } while (aleat >= 8);
    //aleat = 0;


    switch(aleat) {

      case 0:  /* WAIT  */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === WAIT === \n");
#endif

        for(j = 0; j < senders + receivers; j++) {
          rqst_ptr = &rqst[j];
          if(0 > (excpn = wait(&rqst_ptr, &status_any))) {
            fprintf(stdout, "\n:: CR :::::::::: Radiator(%x). (Gix %x, Rank %d): Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            for(i = 0; i < senders; i++)
              free(s[i]);
            return(excpn);
          }
        }
        
        break;

      case 1: /* WAITANY */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === WAITANY === \n");
#endif

        for(k = 0; k < senders + receivers; k++) {
          if(0 > (excpn = waitany(all_rqst, senders + receivers + 2, &idx, &status_any))) {
            fprintf(stderr, "\n:: CR :::::::::: Radiator(%x). [%d  %d]: Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            for(i = 0; i < senders; i++)
              free(s[i]);
            return(excpn);
          }
        }
 
        break;

      case 2: /* WAITALL */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === WAITALL === \n");
#endif

        if(0 > (excpn = waitall(all_rqst, senders + receivers + 2, status))) {

          fprintf(stdout, "\n:: CR :::::::::: Radiator(%x). [%d  %d]: Exception %d :::::::::: \n", (unsigned int)(THR_self()), gix, myid, excpn);
          for(i = 0; i < senders + receivers; i++) {
            fprintf(stdout, ":: CR(%x). Status[%d] = %d\n", (unsigned int)(THR_self()), i, status[i].Error);
            if(status[i].Error != AZQ_SUCCESS)
              return status[i].Error;
          }
          for(i = 0; i < senders; i++)
            free(s[i]);
          return(excpn);
        }

        break;

      case 3: /* WAITSOME */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === WAITSOME === \n");
#endif
        doneCnt = 0;
        
        do {
          
          if(0 > (excpn = waitsome(all_rqst, senders + receivers + 2, index, status, &outCnt))) {
            fprintf(stderr, "\n:: CR :::::::::: Radiator(%x). (Gix %x, Rank %d): Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            for(i = 0; i < senders + receivers; i++) {
              fprintf(stderr, ":: CR(%x). Status[%d] = %d\n", (unsigned int)(THR_self()), i, status[i].Error);
              if(status[i].Error == AZQ_WAIT_ERR_PENDING)
                return excpn;
            }
            for(i = 0; i < senders; i++)
              free(s[i]);
            return(excpn);
          }

          if((senders + receivers) == (doneCnt += outCnt))  break;

        } while(outCnt != AZQ_UNDEFINED);
          
        break;

      case 4:  /* TEST_ */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === TEST === \n");
#endif
        for(j = 0; j < senders + receivers; j++) {
          rqst_ptr = &rqst[j];
          flag = FALSE;
          while (!flag) {
            if(0 > (excpn = test(&rqst_ptr, &flag, &status_any))) {
              fprintf(stdout, "\n:: CR :::::::::: Radiator(%x). (Gix %x, Rank %d): Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
              for(i = 0; i < senders; i++)
                free(s[i]);
              return(excpn);
            }
          }
        }

        break;

      case 5: /* TESTANY */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === TESTANY ===\n");
#endif

        for(k = 0; k < senders + receivers; k++) {
          flag = FALSE;
          while (!flag) {
            if(0 > (excpn = testany(all_rqst, senders + receivers + 2, &idx, &flag, &status_any))) {
              fprintf(stderr, "\n:: CR :::::::::: Radiator(%x). [%d  %d]: Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
              for(i = 0; i < senders; i++)
                free(s[i]);
              return(excpn);
            }
          }
        }
 
        break;

      case 6:  /* TESTALL */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === TESTALL ===\n");
#endif

        flag = FALSE;
        while (!flag) {
          if(0 > (excpn = testall(all_rqst, senders + receivers + 2, &flag, status))) {
            fprintf(stdout, "\n:: CR :::::::::: Radiator(%x). [%d  %d]: Exception %d :::::::::: \n", (unsigned int)(THR_self()), gix, myid, excpn);
            for(i = 0; i < senders + receivers; i++) {
              fprintf(stdout, ":: CR(%x). Status[%d] = %d\n", (unsigned int)(THR_self()), i, status[i].Error);
              if(status[i].Error != AZQ_SUCCESS)
                return status[i].Error;
            }
            for(i = 0; i < senders; i++)
              free(s[i]);
            return(excpn);
          }
        }

        break;

      case 7: /* TESTSOME */
#ifdef __DEBUG
        if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\t\t - CR - === TESTSOME ===\n");
#endif
        doneCnt = 0;
        
        do {
          
          if(0 > (excpn = testsome(all_rqst, senders + receivers + 2, &outCnt, index, status))) {
            fprintf(stderr, "\n:: CR :::::::::: Radiator(%x). (Gix %x, Rank %d): Exception %d :::::::::: \n", (int)(THR_self()), gix, myid, excpn);
            for(i = 0; i < senders + receivers; i++) {
              fprintf(stderr, ":: CR(%x). Status[%d] = %d\n", (unsigned int)(THR_self()), i, status[i].Error);
              if(status[i].Error == AZQ_WAIT_ERR_PENDING)
                return excpn;
            }
            for(i = 0; i < senders; i++)
              free(s[i]);
            return(excpn);
          }

          if((senders + receivers) == (doneCnt += outCnt))  break;
        } while(outCnt != AZQ_UNDEFINED);
          
        break;

      default: fprintf(stdout, "\n:: CR :::::::::: Radiator(%x). [%d  %d]: DEFAULT branch :::::::::: \n", (unsigned int)(THR_self()), gix, myid); 

    }    

#ifdef __CHECK
    /* ---------- 2. Run the ALGORITHM --------- */
    for(j = 0; j < senders; j++)
      for(i = 0; i < bufSize / sizeof(int); i++) {
        if(s[j][i] != j) {
          fprintf(stderr, "\n :::::::::: Radiator (%x). USR %d Fail!!. Itr = %d/Value = %d :::::::::: \n", (int)THR_self(), j, itr, s[j][i]);
          exit(1);
        }
      }
/*
    if(senders) {
        
        acum = 0;
        for(i = 0; i < bufSize / sizeof(int); i++) {
          acum += itr;
        }

        sum = 0;
        for(i = 0; i < bufSize / sizeof(int); i++) {
          for(j = 0; j < senders; j++)
            sum += (s[j][i]);
        }

    /*---------- 3. Print results --------- *
#ifdef __DEBUG
      if(itr % PRINT_PERIOD == 0)  fprintf(stdout, "\n ::::::::::  Adder Operator (group %d).  SUM = %d  :::::::::: \n", gix, sum); 
#endif 

      if(sum != acum * senders) {
        fprintf(stderr, "\n :::::::::: Radiator(%x). Bad SUM = %d  VALUE = %d:::::::::: \n", (int)THR_self(), sum, acum * senders);
        excpn = THR_E_EXHAUST;
        fprintf(stderr, "\n :::::::::: Radiator(%x): Exception %d :::::::::: \n", (int)THR_self(), excpn);
        for(j = 0; j < senders; j++)
          free(s[j]);
        return(excpn);
      }
    }
*/
#endif



      
    if (++itr == ITERA)  break;

  }

  for(j = 0; j < senders; j++) {
    if(s[j])
      free(s[j]);
  }

  free (rqst);

  return 0;
}


     /*----------------------------------------------------------*\
    |    timed_radiator                                            |
    |                                                              |
     \*----------------------------------------------------------*/
int node_main (int argc, char *argv[]) {

  int   myid;
  int   numprocs;
  int   senders;
  int   receivers;
  int   bufsize;
  int   excpn;


  if (argc != 4) {
    fprintf(stderr, "\nUse:  radiator  mess_size  num_senders  num_receivers\n\n");
    exit(1);
  }

  bufsize   = atoi(argv[1]);
  senders   = atoi(argv[2]);
  receivers = atoi(argv[3]);

  GRP_getSize(getGroup(), &numprocs);
  myid = getRank();

  srand(abstime());

#ifdef __DEBUG
  fprintf(stdout, "[%d , %d] %s in a group with [S %d, R %d, C %d]\n", getGroup(), myid, "XXXX", senders, receivers, numprocs - (senders + receivers));
#endif


  if(myid < senders) {
    excpn = do_sender(bufsize, senders, receivers);
    if (0 > excpn) {
      fprintf(stdout, "ERROR: do_sender  %d\n", excpn);
      exit(1);
    }
  } else if (myid == (senders + receivers)) {
    excpn = do_radiator(bufsize, senders, receivers);
    if (0 > excpn) {
      fprintf(stdout, "ERROR: do_radiator  %d\n", excpn);
      exit(1);
    }
  } else {
    excpn = do_receiver(bufsize, senders, receivers);
    if (0 > excpn) {
      fprintf(stdout, "ERROR: do_receiver  %d\n", excpn);
      exit(1);
    }
  }

  return 0;
}

