#include "subvq.h"
#include "dict2pid.h"
#include "lextree.h"
#include "kbcore.h"
#include "kb.h"
#include "ascr.h"
#include <pthread.h>

typedef struct {
  kb_t *kb;
  mdef_t *mdef;
  dict2pid_t *d2p;
} sen_active_args_t;

typedef struct {
  subvq_t* vq;
  mgau_model_t * g;
  int32 beam;
  float32 *feat;
  int32* sen_active;
  int32* senscr;
  
  dict2pid_t *d2p;
  int32 *comsenscr;
} scoring_args_t;

typedef struct {
  kb_t *kb;
  lextree_t *lextree;
  kbcore_t *kbc;
  ascr_t *ascr;
  int32 frm;
  FILE *fp;

  int32 ptranskip;
  vithist_t *vithist;
  int32 th, pth, wth;
  
} searching_args_t;

typedef struct {
  int id;
  int32 feval_start, feval_end;
  int32 com_start, com_end;
  int32 hmmeval_start, hmmeval_end;
  int32 hmmppg_start, hmmppg_end;

  int32 best, wbest; /* hmm_eval results */
  int32 lextree_num;
#ifdef NEW_PAR
  int32 n_mgau, n_comstate;
#endif
  lextree_t* lextree;
  sen_active_args_t *sa_args;
  scoring_args_t *score_args;
  searching_args_t *search_args;
} thread_args_t;

void ThreadJoin();
void threading_support_init(kb_t* kb);
void *frame_eval_thrd_work(thread_args_t* thrd_args);

void dict2pid_comsenscr_thrd_work(thread_args_t* thrd_args);
void lextree_hmm_eval_thrd_work(thread_args_t* thrd_args);
void lextree_hmm_ppg_thrd_work(thread_args_t* thrd_args);
void thrd_scoring_phase(scoring_args_t* score_args);
void new_thrd_lextree_hmm_eval(searching_args_t* search_args, int32* besthmmscr,
			       int32* bestwordscr, int32 *n_hmm_eval, 
			       int32 *frm_nhmm);
void new_lextree_hmm_eval_thrd_work(thread_args_t* thrd_args);
void new_thrd_lextree_hmm_propagate(searching_args_t* search_args);
void new_lextree_hmm_ppg_thrd_work(thread_args_t* thrd_args);

