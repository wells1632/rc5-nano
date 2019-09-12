/*
 * Copyright distributed.net 1997-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * -----------------------------------------------------------------
 * Rewritten from scratch by Cyrus Patel <cyp@fb14.uni-mainz.de>
 * CheckpointAction( int action, unsigned int load_problem_count )
 * is called with action == CHECKPOINT_[OPEN|REFRESH|CLOSE]. It uses the
 * Problem manager table 'load_problem_count' times to walk the problem
 * list when refreshing the checkpoint file. CheckpointAction() returns
 * non-zero if the client should not use checkpointing. The checkpoint
 * interval is controlled from Run()
 * -----------------------------------------------------------------
*/
const char *checkpt_cpp(void) {
  return "@(#)$Id: checkpt.cpp,v 1.26 2008/12/30 20:58:41 andreasb Exp $";
}

#include "client.h"   // FileHeader, Client class
#include "baseincs.h" // memset(), strlen()
#include "util.h"     // IsFilenameValid(), DoesFileExist()
#include "buffbase.h" // Buffer[Zap|Put|Import]FileRecord[s]()
#include "problem.h"  // Problem class
#include "probman.h"  // GetProblemPointerFromIndex()
#include "version.h"  // CLIENT_* defines used by FILEENTRY_* macros
#include "probfill.h" // FILEENTRY_xxx macros
#include "logstuff.h" // LogScreen()
#include "checkpt.h"  // ourselves

// ---------------------------------------------------------------------

// Retrieves, saves, or zaps checkpoint files according to "action".
// The "load_problem_count" is only used to affect checkpoint saving.
// Returns 0 on success, otherwise checkpointing is not enabled.

int CheckpointAction( Client *client, int action, unsigned int load_problem_count )
{
  int do_checkpoint = (client->nodiskbuffers==0);

  if (do_checkpoint)
  {
    unsigned int len = 0;
    while (client->checkpoint_file[len]!=0 &&
           isspace(client->checkpoint_file[len]))
      len++;
    if (len)
      strcpy( client->checkpoint_file, &client->checkpoint_file[len] );
    len = strlen(client->checkpoint_file);
    while (len>0 && isspace(client->checkpoint_file[len-1]))
      client->checkpoint_file[--len]=0;
    do_checkpoint = (client->nodiskbuffers==0 &&
                     client->checkpoint_file[0] &&
                     strcmp(client->checkpoint_file,"none")!=0);
    /* old ini's and command line support for 'ckpoint=none' */
  }

  if ( action == CHECKPOINT_OPEN )
  {
    if (do_checkpoint)
    {
      long recovered = BufferImportFileRecords( client, client->checkpoint_file, 0 );
      if (recovered > 0)
      {
        Log("Recovered %ld checkpoint packet%s\n", recovered,
            ((recovered == 1) ? ("") : ("s")) );
      }
      action = CHECKPOINT_CLOSE;
    }
  }

  /* --------------------------------- */

  if ( action == CHECKPOINT_CLOSE || action == CHECKPOINT_REFRESH )
  {
    if (do_checkpoint)
      BufferZapFileRecords( client->checkpoint_file );
  }

  /* --------------------------------- */

  if ( action == CHECKPOINT_REFRESH )
  {
    if ( do_checkpoint )
    {
      unsigned int prob_i;
      for ( prob_i = 0 ; prob_i < load_problem_count ; prob_i++)
      {
        Problem *thisprob = GetProblemPointerFromIndex(prob_i);
        if ( thisprob )
        {
          if (ProblemIsInitialized(thisprob))
          {
            WorkRecord work;
            unsigned int cont_i;
            memset((void *)&work, 0, sizeof(WorkRecord));
            if (ProblemRetrieveState(thisprob, &work.work, &cont_i, 0, 0) >= 0)
            {
              if (cont_i < CONTEST_COUNT)
              {
                work.resultcode = RESULT_WORKING;
                work.contest = cont_i;
                work.cpu     = FILEENTRY_CPU(thisprob->pub_data.client_cpu);
                work.os      = FILEENTRY_OS;
                work.build   = FILEENTRY_BUILD;
                work.core    = FILEENTRY_CORE(thisprob->pub_data.coresel);
                strncpy( work.id, client->id, sizeof(work.id));
                work.id[sizeof(work.id)-1]=0;

                if (BufferPutFileRecord( client->checkpoint_file, &work,
                                         NULL, BUFFER_FLAGS_CHECKPOINT ) < 0)
                {                        /* returns <0 on ioerr */
                  //Log( "Checkpoint %u, Buffer Error \"%s\"\n",
                  //                     prob_i+1, client->checkpoint_file );
                  //break;
                }
              }
            }
          }
        }
      }  // for ( prob_i = 0 ; prob_i < load_problem_count ; prob_i++)
    } // if ( !client->nodiskbuffers )
  }

  return (do_checkpoint == 0); /* return !0 if don't do checkpoints */
}

// ---------------------------------------------------------------------

