/* ====================================================================
 * Copyright (c) 1999-2001 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/************************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 2000 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 ************************************************
 * 
 * HISTORY
 *
 * 30-Dec-2000  Rita Singh (rsingh@cs.cmu.edu) at Carnegie Mellon University
 *		Added utt_decode_block() to allow block-based decoding 
 *		and decoding of piped input.
 * 
 * 30-Dec-2000  Rita Singh (rsingh@cs.cmu.edu) at Carnegie Mellon University
 *		Moved all utt_*() routines into utt.c to make them independent
 *		of main() during compilation
 * 
 * 29-Feb-2000	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Modified to allow runtime choice between 3-state and 5-state HMM
 * 		topologies (instead of compile-time selection).
 * 
 * 13-Aug-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Added -maxwpf.
 * 
 * 10-May-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Started.
 */

#ifdef THRD
#include "threading.h"

extern lextree_node_t **next_active_array[NUM_THREADS];
extern int32 next_active_size[NUM_THREADS];
extern int32 *he_best_array[NUM_THREADS];
extern int32 *he_wbest_array[NUM_THREADS];
#endif

#ifdef WIN32
#include <direct.h>		/* RAH, added */
#endif

#include "kb.h"
#include "corpus.h"
#include "utt.h"
#include "subvq.h"


void matchseg_write (FILE *fp, kb_t *kb, glist_t hyp, char *hdr)
{
    gnode_t *gn;
    hyp_t *h;
    int32 ascr, lscr;
    dict_t *dict;
    
    ascr = 0;
    lscr = 0;
    
    for (gn = hyp; gn; gn = gnode_next(gn)) {
	h = (hyp_t *) gnode_ptr (gn);
	ascr += h->ascr;
	lscr += h->lscr;
    }
    
    dict = kbcore_dict(kb->kbcore);
    
    fprintf (fp, "%s%s S 0 T %d A %d L %d", (hdr ? hdr : ""), kb->uttid, ascr+lscr, ascr, lscr);
    for (gn = hyp; gn && (gnode_next(gn)); gn = gnode_next(gn)) {
	h = (hyp_t *) gnode_ptr (gn);
	fprintf (fp, " %d %d %d %s", h->sf, h->ascr, h->lscr, dict_wordstr (dict, h->id));
    }
    fprintf (fp, " %d\n", kb->nfr);
    fflush (fp);
}
/*
 * Begin search at bigrams of <s>, backing off to unigrams; and fillers.  Update
 * kb->lextree_next_active with the list of active lextrees.
 */
void utt_begin (kb_t *kb)
{
    kbcore_t *kbc;
    int32 n, pred;
    
    kbc = kb->kbcore;
    
    /* Insert initial <s> into vithist structure */
    pred = vithist_utt_begin (kb->vithist, kbc);
    assert (pred == 0);	/* Vithist entry ID for <s> */
    
    /* Enter into unigram lextree[0] */
    n = lextree_n_next_active(kb->ugtree[0]);
    assert (n == 0);
    lextree_enter (kb->ugtree[0], mdef_silphone(kbc->mdef), -1, 0, pred, kb->beam->hmm);
    
    /* Enter into filler lextree */
    n = lextree_n_next_active(kb->fillertree[0]);
    assert (n == 0);
    lextree_enter (kb->fillertree[0], BAD_S3CIPID, -1, 0, pred, kb->beam->hmm);
    
    kb->n_lextrans = 1;
    
    kb_lextree_active_swap (kb);

#ifdef THRD
    threading_support_init(kb);

#endif
}

void utt_end (kb_t *kb)
{
    int32 id, ascr, lscr;
    glist_t hyp;
    gnode_t *gn;
    hyp_t *h;
    FILE *fp, *latfp;
    dict_t *dict;
    int32 i;
    
    fp = stderr;
    dict = kbcore_dict (kb->kbcore);
    
    if ((id = vithist_utt_end (kb->vithist, kb->kbcore)) >= 0) {
      if (cmd_ln_str("-bptbldir")) {
	char file[8192];
	
	sprintf (file, "%s/%s.bpt", cmd_ln_str ("-bptbldir"), kb->uttid);
	if ((latfp = fopen (file, "w")) == NULL) {
	  E_ERROR("fopen(%s,w) failed; using stdout\n", file);
	  latfp = stdout;
	}
	
	vithist_dump (kb->vithist, -1, kb->kbcore, latfp);
	if (latfp != stdout)
	  fclose (latfp);
      }
      
      hyp = vithist_backtrace (kb->vithist, id);
      
      /* Detailed backtrace */
      fprintf (fp, "\nBacktrace(%s)\n", kb->uttid);
      fprintf (fp, "%6s %5s %5s %11s %8s %4s\n",
	       "LatID", "SFrm", "EFrm", "AScr", "LScr", "Type");
      
      ascr = 0;
      lscr = 0;
      
      for (gn = hyp; gn; gn = gnode_next(gn)) {
	h = (hyp_t *) gnode_ptr (gn);
	fprintf (fp, "%6d %5d %5d %11d %8d %4d %s\n",
		 h->vhid, h->sf, h->ef, h->ascr, h->lscr, h->type,
		 dict_wordstr(dict, h->id));
	
	ascr += h->ascr;
	lscr += h->lscr;
      }
      fprintf (fp, "       %5d %5d %11d %8d (Total)\n",0,kb->nfr,ascr,lscr);
      
      /* Match */
      fprintf (fp, "\nFWDVIT: ");
      for (gn = hyp; gn; gn = gnode_next(gn)) {
	h = (hyp_t *) gnode_ptr (gn);
	if((!dict_filler_word(dict,h->id)) && (h->id!=dict_finishwid(dict)))
	  fprintf(fp,"%s ",dict_wordstr(dict, dict_basewid(dict,h->id)));
      }
      fprintf (fp, " (%s)\n\n", kb->uttid);
      
      /* Matchseg */
      if (kb->matchsegfp)
	matchseg_write (kb->matchsegfp, kb, hyp, NULL);
      matchseg_write (fp, kb, hyp, "FWDXCT: ");
      fprintf (fp, "\n");
      
      if (cmd_ln_str ("-outlatdir")) {
	char str[16384];
	int32 ispipe;
	float64 logbase;
	
	sprintf (str, "%s/%s.%s",
		 cmd_ln_str("-outlatdir"), kb->uttid, cmd_ln_str("-latext"));
	E_INFO("Writing lattice file: %s\n", str);
	
	if ((latfp = fopen_comp (str, "w", &ispipe)) == NULL)
	  E_ERROR("fopen_comp (%s,w) failed\n", str);
	else {
	  /* Write header info */
	  assert(0&&"used to be getcwd");
	  /*getcwd (str, sizeof(str));*/
	  fprintf (latfp, "# getcwd: %s\n", str);
	  
	  /* Print logbase first!!  Other programs look for it early in the DAG */
	  logbase = cmd_ln_float32 ("-logbase");
	  fprintf (latfp, "# -logbase %e\n", logbase);
	  
	  fprintf (latfp, "# -dict %s\n", cmd_ln_str ("-dict"));
	  if (cmd_ln_str ("-fdict"))
	    fprintf (latfp, "# -fdict %s\n", cmd_ln_str ("-fdict"));
	  fprintf (latfp, "# -lm %s\n", cmd_ln_str ("-lm"));
	  fprintf (latfp, "# -mdef %s\n", cmd_ln_str ("-mdef"));
	  fprintf (latfp, "# -mean %s\n", cmd_ln_str ("-mean"));
	  fprintf (latfp, "# -var %s\n", cmd_ln_str ("-var"));
	  fprintf (latfp, "# -mixw %s\n", cmd_ln_str ("-mixw"));
	  fprintf (latfp, "# -tmat %s\n", cmd_ln_str ("-tmat"));
	  fprintf (latfp, "#\n");
	  
	  fprintf (latfp, "Frames %d\n", kb->nfr);
	  fprintf (latfp, "#\n");
	  
	  vithist_dag_write (kb->vithist, hyp, dict, cmd_ln_int32("-outlatoldfmt"), latfp);
	  fclose_comp (latfp, ispipe);
	}
      }
      
      /* Free hyplist */
      for (gn = hyp; gn && (gnode_next(gn)); gn = gnode_next(gn)) {
	h = (hyp_t *) gnode_ptr (gn);
	ckd_free ((void *) h);
      }
      glist_free (hyp);
    } else
      E_ERROR("%s: No recognition\n\n", kb->uttid);
    
#if 0
    E_INFO("%4d frm;  %4d sen, %5d gau/fr, %4.1f CPU %4.1f Clk;  %5d hmm, %3d wd/fr, %4.1f CPU %4.1f Clk (%s)\n",
	   kb->nfr,
	   (kb->utt_sen_eval + (kb->nfr >> 1)) / kb->nfr,
	   (kb->utt_gau_eval + (kb->nfr >> 1)) / kb->nfr,
	   kb->tm_sen.t_cpu * 100.0 / kb->nfr, 
	   kb->tm_sen.t_elapsed * 100.0 / kb->nfr,
	   (kb->utt_hmm_eval + (kb->nfr >> 1)) / kb->nfr,
	   (vithist_n_entry(kb->vithist) + (kb->nfr >> 1)) / kb->nfr,
	   kb->tm_srch.t_cpu * 100.0 / kb->nfr, 
	   kb->tm_srch.t_elapsed * 100.0 / kb->nfr,
	   kb->uttid);
#endif
    E_INFO("%4d frm;  %4d sen, %5d gau/fr, %4.1f CPU %4.1f Clk;  %5d hmm, %3d wd/fr, %4.1f CPU %4.1f Clk (%s)\n",
	   kb->nfr,
	   (kb->utt_sen_eval + (kb->nfr >> 1)) / kb->nfr,
	   (kb->utt_gau_eval + (kb->nfr >> 1)) / kb->nfr,
	   0.0,
	   0.0, /*kb->tm_sen.t_elapsed * 100.0 / kb->nfr,*/
	   (kb->utt_hmm_eval + (kb->nfr >> 1)) / kb->nfr,
	   (vithist_n_entry(kb->vithist) + (kb->nfr >> 1)) / kb->nfr,
	   0.0,
	   0.0, /*kb->tm_srch.t_elapsed * 100.0 / kb->nfr,*/
	   kb->uttid);
    {
      int32 j, k;
      
      for (j = kb->hmm_hist_bins-1; (j >= 0) && (kb->hmm_hist[j] == 0); --j);
      E_INFO("HMMHist[0..%d](%s):", j, kb->uttid);
      for (i = 0, k = 0; i <= j; i++) {
	k += kb->hmm_hist[i];
	fprintf (stderr, " %d(%d)", kb->hmm_hist[i], (k*100)/kb->nfr);
      }
      fprintf (stderr, "\n");
      fflush (stderr);
    }
    
    kb->tot_sen_eval += kb->utt_sen_eval;
    kb->tot_gau_eval += kb->utt_gau_eval;
    kb->tot_hmm_eval += kb->utt_hmm_eval;
    kb->tot_wd_exit += vithist_n_entry(kb->vithist);

    ptmr_reset (&(kb->tm_sen));
    ptmr_reset (&(kb->tm_srch));

#if (!defined(WIN32) && !defined(sparc))
    system ("ps aguxwww | grep /live | grep -v grep"); 
#endif
    for (i = 0; i < kb->n_lextree; i++) {
      lextree_utt_end (kb->ugtree[i], kb->kbcore);
      lextree_utt_end (kb->fillertree[i], kb->kbcore);
    }
    
    vithist_utt_reset (kb->vithist);
    
    lm_cache_stats_dump (kbcore_lm(kb->kbcore));
    lm_cache_reset (kbcore_lm(kb->kbcore));
}


void utt_word_trans (kb_t *kb, int32 cf)
{
  int32 k, th;
  vithist_t *vh;
  vithist_entry_t *ve;
  int32 vhid, le, n_ci, score;
  static int32 *bs = NULL, *bv = NULL, epl;
  s3wid_t wid;
  int32 p;
  dict_t *dict;
  mdef_t *mdef;
  
  vh = kb->vithist;
  th = kb->bestscore + kb->beam->hmm;	/* Pruning threshold */
  
  if (vh->bestvh[cf] < 0)
    return;
  
  dict = kbcore_dict(kb->kbcore);
  mdef = kbcore_mdef(kb->kbcore);
  n_ci = mdef_n_ciphone(mdef);
  
  /* Initialize best exit for each distinct word-final CIphone to NONE */
  if (! bs) {
    bs = (int32 *) ckd_calloc (n_ci, sizeof(int32));
    bv = (int32 *) ckd_calloc (n_ci, sizeof(int32));
    epl = cmd_ln_int32 ("-epl");
  }
  for (p = 0; p < n_ci; p++) {
    bs[p] = MAX_NEG_INT32;
    bv[p] = -1;
  }
  
  /* Find best word exit in this frame for each distinct word-final CI phone */
  vhid = vithist_first_entry (vh, cf);
  le = vithist_n_entry (vh) - 1;
  for (; vhid <= le; vhid++) {
    ve = vithist_id2entry (vh, vhid);
    if (! vithist_entry_valid(ve))
      continue;
    
    wid = vithist_entry_wid (ve);
    p = dict_last_phone (dict, wid);
    if (mdef_is_fillerphone(mdef, p))
      p = mdef_silphone(mdef);
    
    score = vithist_entry_score (ve);
    if (score > bs[p]) {
      bs[p] = score;
      bv[p] = vhid;
    }
  }
  
  /* Find lextree instance to be entered */
  k = kb->n_lextrans++;
  k = (k % (kb->n_lextree * epl)) / epl;
  
  /* Transition to unigram lextrees */
  for (p = 0; p < n_ci; p++) {
    if (bv[p] >= 0)
      lextree_enter (kb->ugtree[k], (s3cipid_t) p, cf, bs[p], bv[p], th); /* RAH, typecast p to (s3cipid_t) to make compiler happy */
  }
  
  /* Transition to filler lextrees */
  lextree_enter (kb->fillertree[k], BAD_S3CIPID, cf, vh->bestscore[cf], vh->bestvh[cf], th);
}


/* Invoked by ctl_process in libmisc/corpus.c */
void utt_decode (void *data, char *uttfile, int32 sf, int32 ef, char *uttid)
{
  kb_t *kb;
  kbcore_t *kbcore;
  mdef_t *mdef;
  dict_t *dict;
  dict2pid_t *d2p;
  mgau_model_t *mgau;
  subvq_t *svq;
  lextree_t *lextree;
  int32 besthmmscr, bestwordscr, th, pth, wth, maxwpf, maxhistpf, maxhmmpf, ptranskip;
  int32 i, j, f;
  int32 n_hmm_eval, frm_nhmm, hb, pb, wb;
  FILE *hmmdumpfp;
  
  E_INFO("Processing: %s\n", uttid);

  kb = (kb_t *) data;
  kbcore = kb->kbcore;
  mdef = kbcore_mdef (kbcore);
  dict = kbcore_dict (kbcore);
  d2p = kbcore_dict2pid (kbcore);
  mgau = kbcore_mgau (kbcore);
  svq = kbcore_svq (kbcore);
  
  kb->uttid = uttid;
  
  hmmdumpfp = cmd_ln_int32("-hmmdump") ? stderr : NULL;
  maxwpf = cmd_ln_int32 ("-maxwpf");
  maxhistpf = cmd_ln_int32 ("-maxhistpf");
  maxhmmpf = cmd_ln_int32 ("-maxhmmpf");
  ptranskip = cmd_ln_int32 ("-ptranskip");
  
  /* Read mfc file and build feature vectors for entire utterance */
  kb->nfr = feat_s2mfc2feat(kbcore_fcb(kbcore), uttfile, cmd_ln_str("-cepdir"), sf, ef,
			    kb->feat, S3_MAX_FRAMES);
  E_INFO("%s: %d frames\n", kb->uttid, kb->nfr);
  
  for (i = 0; i < kb->hmm_hist_bins; i++)
    kb->hmm_hist[i] = 0;
  
  utt_begin (kb);
  
  n_hmm_eval = 0;
  kb->utt_sen_eval = 0;
  kb->utt_gau_eval = 0;
  
  for (f = 0; f < kb->nfr; f++) {
    /* Acoustic (senone scores) evaluation */
    ptmr_start (&(kb->tm_sen));
    
    /* Find active senones and composite senones, from active lextree nodes */
    if (kb->sen_active) {
      memset (kb->ssid_active, 0, mdef_n_sseq(mdef) * sizeof(int32));
      memset (kb->comssid_active, 0, dict2pid_n_comsseq(d2p) * sizeof(int32));
      /* Find active senone-sequence IDs (including composite ones) */
      for (i = 0; i < (kb->n_lextree <<1); i++) {
	lextree = (i < kb->n_lextree) ? kb->ugtree[i] :
	  kb->fillertree[i - kb->n_lextree];
	lextree_ssid_active (lextree, kb->ssid_active, kb->comssid_active);
      }
      
      /* Find active senones from active senone-sequences */
      memset (kb->sen_active, 0, mdef_n_sen(mdef) * sizeof(int32));
      mdef_sseq2sen_active (mdef, kb->ssid_active, kb->sen_active);
      
      /* Add in senones needed for active composite senone-sequences */
      dict2pid_comsseq2sen_active (d2p, mdef, kb->comssid_active, kb->sen_active);
    }
    
    /* Evaluate senone acoustic scores for the active senones */
    subvq_frame_eval (svq, mgau, kb->beam->subvq, kb->feat[f][0], kb->sen_active,
		      kb->ascr->sen);
    kb->utt_sen_eval += mgau_frm_sen_eval(mgau);
    kb->utt_gau_eval += mgau_frm_gau_eval(mgau);
    
    /* Evaluate composite senone scores from senone scores */
    dict2pid_comsenscr (kbcore_dict2pid(kbcore), kb->ascr->sen, kb->ascr->comsen);

    ptmr_stop (&(kb->tm_sen));
    
    /* Search */
    ptmr_start (&(kb->tm_srch));
    
    /* Evaluate active HMMs in each lextree; note best HMM state score */
    besthmmscr = MAX_NEG_INT32;
    bestwordscr = MAX_NEG_INT32;
    frm_nhmm = 0;
    for (i = 0; i < (kb->n_lextree <<1); i++) {
      lextree = (i < kb->n_lextree) ? kb->ugtree[i] : kb->fillertree[i - kb->n_lextree];
      
      if (hmmdumpfp != NULL)
	fprintf (hmmdumpfp, "Fr %d Lextree %d #HMM %d\n", f, i, lextree->n_active);
      
      lextree_hmm_eval (lextree, kbcore, kb->ascr, f, hmmdumpfp);
      
      if (besthmmscr < lextree->best)
	besthmmscr = lextree->best;
      if (bestwordscr < lextree->wbest)
	bestwordscr = lextree->wbest;
      
      n_hmm_eval += lextree->n_active;
      frm_nhmm += lextree->n_active;
    }
    if (besthmmscr > 0) {
      E_ERROR("***ERROR*** Fr %d, best HMM score > 0 (%d); int32 wraparound?\n",
	      f, besthmmscr);
    }
    
    kb->hmm_hist[frm_nhmm / kb->hmm_hist_binsize]++;
    
    /* Set pruning threshold depending on whether number of active HMMs within limit */
    if (frm_nhmm > (maxhmmpf + (maxhmmpf >> 1))) {
      int32 *bin, nbin, bw;
      
      /* Use histogram pruning */
      nbin = 1000;
      bw = -(kb->beam->hmm) / nbin;
      bin = (int32 *) ckd_calloc (nbin, sizeof(int32));
      
      for (i = 0; i < (kb->n_lextree <<1); i++) {
	lextree = (i < kb->n_lextree) ?
	  kb->ugtree[i] : kb->fillertree[i - kb->n_lextree];
	
	lextree_hmm_histbin (lextree, besthmmscr, bin, nbin, bw);
      }
      
      for (i = 0, j = 0; (i < nbin) && (j < maxhmmpf); i++, j += bin[i]);
      ckd_free ((void *) bin);
      
      /* Determine hmm, phone, word beams */
      hb = -(i * bw);
      pb = (hb > kb->beam->ptrans) ? hb : kb->beam->ptrans;
      wb = (hb > kb->beam->word) ? hb : kb->beam->word;

    } else {
      hb = kb->beam->hmm;
      pb = kb->beam->ptrans;
      wb = kb->beam->word;
    }
    
    kb->bestscore = besthmmscr;
    kb->bestwordscore = bestwordscr;
    th = kb->bestscore + hb;		/* HMM survival threshold */
    pth = kb->bestscore + pb;		/* Cross-HMM transition threshold */
    wth = kb->bestwordscore + wb;		/* Word exit threshold */
    
    /*
     * For each lextree, determine if the active HMMs remain active for next
     * frame, propagate scores across HMM boundaries, and note word exits.
     */
    for (i = 0; i < (kb->n_lextree <<1); i++) {
      lextree = (i < kb->n_lextree) ? kb->ugtree[i] : kb->fillertree[i - kb->n_lextree];
      
      /* Hack!! Use a narrow phone transition beam (wth) every so many frames */
      if ((ptranskip < 1) || ((f % ptranskip) != 0))
	lextree_hmm_propagate (lextree, kbcore, kb->vithist, f, th, pth, wth);
      else
	lextree_hmm_propagate (lextree, kbcore, kb->vithist, f, th, wth, wth);
    }
    
    /* Limit vithist entries created this frame to specified max */
    vithist_prune (kb->vithist, dict, f, maxwpf, maxhistpf, wb);
    
    /* Cross-word transitions */
    utt_word_trans (kb, f);
    
    /* Wind up this frame */
    vithist_frame_windup (kb->vithist, f, NULL, kbcore);
    
    kb_lextree_active_swap (kb);

    ptmr_stop (&(kb->tm_srch));
    
    if ((f % 100) == 0) {
      fprintf (stderr, ".");
      fflush (stderr);
    }
  }
  
  kb->utt_hmm_eval = n_hmm_eval;
  
  utt_end (kb);
  kb->tot_fr += kb->nfr;
  
  fprintf (stdout, "\n");
}

#ifdef THRD
sen_active_args_t sen_active_args;
scoring_args_t scoring_args;
searching_args_t searching_args;
#endif

/* This function decodes a block of incoming feature vectors.
 * Feature vectors have to be computed by the calling routine.
 * The utterance level index of the last feature vector decoded
 * (before the current block) must be passed. 
 * The current status of the decode is stored in the kb structure that 
 * is passed in.
 */

void utt_decode_block (float **block_feat,   /* Incoming block of featurevecs */
		       int32 block_nfeatvec, /* No. of vecs in cepblock */
		       int32 *curfrm,	     /* Utterance level index of
						frames decoded so far */
		       kb_t *kb,	     /* kb structure with all model
						and decoder info */
		       int32 maxwpf,	     /* Max words per frame */
		       int32 maxhistpf,	     /* Max histories per frame */
		       int32 maxhmmpf,	     /* Max active HMMs per frame */
		       int32 ptranskip,	     /* intervals at which wbeam
						is used for phone transitions */
		       FILE *hmmdumpfp)      /* dump file */
{
  kbcore_t *kbcore;
  mdef_t *mdef;
  dict_t *dict;
  dict2pid_t *d2p;
  mgau_model_t *mgau;
  subvq_t *svq;
  lextree_t *lextree;
  int32 besthmmscr, bestwordscr, th, pth, wth; 
  int32  i, j, t;
  int32  n_hmm_eval;
  int32 frmno; 
  int32 frm_nhmm, hb, pb, wb;
  
  kbcore = kb->kbcore;
  mdef = kbcore_mdef (kbcore);
  dict = kbcore_dict (kbcore);
  d2p = kbcore_dict2pid (kbcore);
  mgau = kbcore_mgau (kbcore);
  svq = kbcore_svq (kbcore);
  
  frmno = *curfrm;

#if 1
  memset(kb->hmm_hist, 0, kb->hmm_hist_bins*sizeof(int32));
#else
  for (i = 0; i < kb->hmm_hist_bins; i++)
    kb->hmm_hist[i] = 0;
#endif
  n_hmm_eval = 0;

  /*  E_INFO("going through %d vectors\n",block_nfeatvec);*/
  for (t = 0; t < block_nfeatvec; t++,frmno++) {
    /* Acoustic (senone scores) evaluation */

    ptmr_start (&(kb->tm_sen));
    
    /* Find active and composite senones from active lextree nodes */
    if (kb->sen_active) {

#if defined(THRD)
      sen_active_args.kb = kb;
      sen_active_args.mdef = mdef;
      sen_active_args.d2p = d2p;
      
      thrd_sen_active_phase(&sen_active_args);
#else
      memset(kb->ssid_active, 0, mdef_n_sseq(mdef) * sizeof(int32));
      memset(kb->comssid_active,0,dict2pid_n_comsseq(d2p)*sizeof(int32));
      /* moved from before mdef_sseq2sen*/
      memset (kb->sen_active, 0, mdef_n_sen(mdef) * sizeof(int32));

      /* Find active senone-sequence IDs (including composite ones) */
      for (i = 0; i < (kb->n_lextree <<1); i++) {
	lextree = (i < kb->n_lextree) ? kb->ugtree[i] :
	  kb->fillertree[i - kb->n_lextree];
	lextree_ssid_active(lextree,kb->ssid_active,kb->comssid_active);
      }
      
      /* Find active senones from active senone-sequences */
      mdef_sseq2sen_active (mdef, kb->ssid_active, kb->sen_active);
      
      /* Add in senones needed for active composite senone-sequences */
      dict2pid_comsseq2sen_active (d2p, mdef, kb->comssid_active, 
				   kb->sen_active);
#endif
    } else 
      assert(0&&"!sen_active\n");

    /* Evaluate senone acoustic scores for the active senones */
#if defined(THRD) 
  scoring_args.vq = svq;
  scoring_args.g = mgau;
  scoring_args.beam = kb->beam->subvq;
  scoring_args.feat = block_feat[t];
  scoring_args.sen_active = kb->sen_active;
  scoring_args.senscr = kb->ascr->sen;

  scoring_args.d2p = d2p;
  scoring_args.comsenscr = kb->ascr->comsen;

  thrd_scoring_phase(&scoring_args);

  kb->utt_sen_eval += mgau_frm_sen_eval(mgau);
  kb->utt_gau_eval += mgau_frm_gau_eval(mgau);
    
#else
  subvq_frame_eval (svq, mgau, kb->beam->subvq, block_feat[t], 
		    kb->sen_active, kb->ascr->sen);
  
  kb->utt_sen_eval += mgau_frm_sen_eval(mgau);
  kb->utt_gau_eval += mgau_frm_gau_eval(mgau);
  
  /*E_INFO("d2p->n_comstate %d\n",d2p->n_comstate);*/
  /* Evaluate composite senone scores from senone scores */

  dict2pid_comsenscr (d2p, kb->ascr->sen, 	
		      kb->ascr->comsen);

#endif
  if (DEBUG&0x1) {
    fprintf(stderr,"%d comstate\n",d2p->n_comstate);
    for (i=0;i<d2p->n_comstate;i++)
      fprintf(stderr,"%d %d\n",i,kb->ascr->comsen[i]);
  }

    ptmr_stop (&(kb->tm_sen));
    
    /* Search */
    ptmr_start (&(kb->tm_srch));


    /* Evaluate active HMMs in each lextree; note best HMM state score */
    besthmmscr = MAX_NEG_INT32;
    bestwordscr = MAX_NEG_INT32;
    frm_nhmm = 0;

    if (DEBUG&0x1) fprintf(stderr,"gonna search in %d trees\n",kb->n_lextree);

#if defined(THRD) && (PHASES & 0x2)
    searching_args.kbc = kbcore;
    searching_args.ascr = kb->ascr;
    searching_args.frm = frmno;
    searching_args.fp = hmmdumpfp;
    searching_args.kb = kb;

    new_thrd_lextree_hmm_eval(&searching_args, &besthmmscr, &bestwordscr, 
			      &n_hmm_eval, &frm_nhmm);
#else
    for (i = 0; i < (kb->n_lextree <<1); i++) {
      lextree = (i < kb->n_lextree) ? kb->ugtree[i] : 
	kb->fillertree[i - kb->n_lextree];
      
      if (hmmdumpfp != NULL)
	fprintf (hmmdumpfp, "Fr %d Lextree %d #HMM %d\n", frmno, i, 
		 lextree->n_active);

      lextree_hmm_eval (lextree, kbcore, kb->ascr, frmno, hmmdumpfp);

      if (besthmmscr < lextree->best)
	besthmmscr = lextree->best;
      if (bestwordscr < lextree->wbest)
	bestwordscr = lextree->wbest;
      
      n_hmm_eval += lextree->n_active;
      frm_nhmm += lextree->n_active;
    }
#endif
    if (DEBUG&0x1) {
      fprintf(stderr,"n_hmm_eval %d frm_nhmm %d besthmmscr %d bestwordscr %d\n",
	      n_hmm_eval, frm_nhmm, besthmmscr, bestwordscr);
    }

#if defined(THRD) && (!(PHASES & 0x2)) && (PHASES&0x4)
    searching_args.kbc = kbcore;
    searching_args.ascr = kb->ascr;
    searching_args.frm = frmno;
    searching_args.fp = hmmdumpfp;
    searching_args.kb = kb;
#endif
    
    if (besthmmscr > 0) {
      E_ERROR("***ERROR*** Fr %d, best HMM score > 0 (%d); int32 wraparound?\n",
	      frmno, besthmmscr);
    }

    kb->hmm_hist[frm_nhmm / kb->hmm_hist_binsize]++;
    
    /* Set pruning threshold depending on whether number of active HMMs 
     * is within limit 
     */
    if (frm_nhmm > (maxhmmpf + (maxhmmpf >> 1))) {
      int32 *bin, nbin, bw;
      
      /* Use histogram pruning */
      nbin = 1000;
      bw = -(kb->beam->hmm) / nbin;
      bin = (int32 *) ckd_calloc (nbin, sizeof(int32));
      
      for (i = 0; i < (kb->n_lextree <<1); i++) {
	lextree = (i < kb->n_lextree) ?
	  kb->ugtree[i] : kb->fillertree[i - kb->n_lextree];
	
	lextree_hmm_histbin (lextree, besthmmscr, bin, nbin, bw);
      }
      
      for (i = 0, j = 0; (i < nbin) && (j < maxhmmpf); i++, j += bin[i]);
      ckd_free ((void *) bin);
      
      /* Determine hmm, phone, word beams */
      hb = -(i * bw);
      pb = (hb > kb->beam->ptrans) ? hb : kb->beam->ptrans;
      wb = (hb > kb->beam->word) ? hb : kb->beam->word;
    } else {
      hb = kb->beam->hmm;
      pb = kb->beam->ptrans;
      wb = kb->beam->word;
    }
    
    kb->bestscore = besthmmscr;
    kb->bestwordscore = bestwordscr;
    th = kb->bestscore + hb;	/* HMM survival threshold */
    pth = kb->bestscore + pb;	/* Cross-HMM transition threshold */
    wth = kb->bestwordscore + wb;	/* Word exit threshold */
    
#if defined(THRD ) && (PHASES & 0x4)
    searching_args.ptranskip = ptranskip;
    searching_args.vithist = kb->vithist;
    searching_args.th = th;
#if 0 /* To prevent divide-by-zero exception in intel cc */
    if ((ptranskip < 1) || ((frmno % (ptranskip==0)?1:ptranskip) != 0))
#else
    if ((ptranskip < 1) || ((frmno % ptranskip) != 0))
#endif
      searching_args.pth = pth;
    else
      searching_args.pth = wth;
    searching_args.wth = wth;

    new_thrd_lextree_hmm_propagate(&searching_args);
    
#else
    /*
     * For each lextree, determine if the active HMMs remain active for next
     * frame, propagate scores across HMM boundaries, and note word exits.
     */

    for (i = 0; i < (kb->n_lextree <<1); i++) {
      lextree = (i < kb->n_lextree) ? kb->ugtree[i] : 
	kb->fillertree[i - kb->n_lextree];

      /* Hack! Use narrow phone transition beam (wth) every few frames */

#if 1 /* To prevent divide-by-zero exception in intel cc */
      if ((ptranskip < 1) || ((frmno % (ptranskip==0)?1:ptranskip) != 0))
#else
      if ((ptranskip < 1) || ((frmno % ptranskip) != 0))
#endif
	lextree_hmm_propagate(lextree, kbcore, kb->vithist, frmno,
			      th, pth, wth);
      else
	lextree_hmm_propagate(lextree, kbcore, kb->vithist, frmno,
			      th, wth, wth);
    }
#endif
    /* if threaded with more than 1 thread and last phase is turned
       off, then thread join is needed here. Else, thread join is
       done inside new_thrd_lextree_hmm_propagate */
  
    /* Limit vithist entries created this frame to specified max */
    vithist_prune (kb->vithist, dict, frmno, maxwpf, maxhistpf, wb);

    /* Cross-word transitions */
    utt_word_trans (kb, frmno);

    /* Wind up this frame */
    vithist_frame_windup (kb->vithist, frmno, NULL, kbcore);

    kb_lextree_active_swap (kb);

    ptmr_stop (&(kb->tm_srch));

  }

  kb->utt_hmm_eval += n_hmm_eval;
  kb->nfr += block_nfeatvec;
  
  *curfrm = frmno;
}

void single_utt_decode_block (float **block_feat,   /* Incoming block of featurevecs */
		       int32 block_nfeatvec, /* No. of vecs in cepblock */
		       int32 *curfrm,	     /* Utterance level index of
						frames decoded so far */
		       kb_t *kb,	     /* kb structure with all model
						and decoder info */
		       int32 maxwpf,	     /* Max words per frame */
		       int32 maxhistpf,	     /* Max histories per frame */
		       int32 maxhmmpf,	     /* Max active HMMs per frame */
		       int32 ptranskip,	     /* intervals at which wbeam
						is used for phone transitions */
		       FILE *hmmdumpfp)      /* dump file */
{
  kbcore_t *kbcore;
  mdef_t *mdef;
  dict_t *dict;
  dict2pid_t *d2p;
  mgau_model_t *mgau;
  subvq_t *svq;
  lextree_t *lextree;
  int32 besthmmscr, bestwordscr, th, pth, wth; 
  int32  i, j, t;
  int32  n_hmm_eval;
  int32 frmno; 
  int32 frm_nhmm, hb, pb, wb;
  
  kbcore = kb->kbcore;
  mdef = kbcore_mdef (kbcore);
  dict = kbcore_dict (kbcore);
  d2p = kbcore_dict2pid (kbcore);
  mgau = kbcore_mgau (kbcore);
  svq = kbcore_svq (kbcore);
  
  frmno = *curfrm;
#if 1
  memset(kb->hmm_hist, 0, kb->hmm_hist_bins*sizeof(int32));
#else
  for (i = 0; i < kb->hmm_hist_bins; i++)
    kb->hmm_hist[i] = 0;
#endif
  n_hmm_eval = 0;

  /*  E_INFO("going through %d vectors\n",block_nfeatvec);*/
  for (t = 0; t < block_nfeatvec; t++,frmno++) {
    /* Acoustic (senone scores) evaluation */

    ptmr_start (&(kb->tm_sen));
    
    /* Find active and composite senones from active lextree nodes */
    if (kb->sen_active) {
      memset(kb->ssid_active, 0, mdef_n_sseq(mdef) * sizeof(int32));
      memset(kb->comssid_active,0,dict2pid_n_comsseq(d2p)*sizeof(int32));
      /* moved from before mdef_sseq2sen*/
      memset (kb->sen_active, 0, mdef_n_sen(mdef) * sizeof(int32));

      /* Find active senone-sequence IDs (including composite ones) */
      for (i = 0; i < (kb->n_lextree <<1); i++) {
	lextree = (i < kb->n_lextree) ? kb->ugtree[i] :
	  kb->fillertree[i - kb->n_lextree];
	lextree_ssid_active(lextree,kb->ssid_active,kb->comssid_active);
      }
      
      /* Find active senones from active senone-sequences */
      mdef_sseq2sen_active (mdef, kb->ssid_active, kb->sen_active);
      
      /* Add in senones needed for active composite senone-sequences */
      dict2pid_comsseq2sen_active (d2p, mdef, kb->comssid_active, 
				   kb->sen_active);
    } else 
      assert(0&&"!sen_active\n");


    /* Evaluate senone acoustic scores for the active senones */
    subvq_frame_eval (svq, mgau, kb->beam->subvq, block_feat[t], 
		    kb->sen_active, kb->ascr->sen);
  
  kb->utt_sen_eval += mgau_frm_sen_eval(mgau);
  kb->utt_gau_eval += mgau_frm_gau_eval(mgau);
  
  /*E_INFO("d2p->n_comstate %d\n",d2p->n_comstate);*/
  /* Evaluate composite senone scores from senone scores */

  dict2pid_comsenscr (d2p, kb->ascr->sen, 	
		      kb->ascr->comsen);

    ptmr_stop (&(kb->tm_sen));
    
    /* Search */
    ptmr_start (&(kb->tm_srch));

    /* Evaluate active HMMs in each lextree; note best HMM state score */
    besthmmscr = MAX_NEG_INT32;
    bestwordscr = MAX_NEG_INT32;
    frm_nhmm = 0;

    for (i = 0; i < (kb->n_lextree <<1); i++) {
      lextree = (i < kb->n_lextree) ? kb->ugtree[i] : 
	kb->fillertree[i - kb->n_lextree];
      
      if (hmmdumpfp != NULL)
	fprintf (hmmdumpfp, "Fr %d Lextree %d #HMM %d\n", frmno, i, 
		 lextree->n_active);

      lextree_hmm_eval (lextree, kbcore, kb->ascr, frmno, hmmdumpfp);

      if (besthmmscr < lextree->best)
	besthmmscr = lextree->best;
      if (bestwordscr < lextree->wbest)
	bestwordscr = lextree->wbest;
      
      n_hmm_eval += lextree->n_active;
      frm_nhmm += lextree->n_active;
    }
    
    if (besthmmscr > 0) {
      E_ERROR("***ERROR*** Fr %d, best HMM score > 0 (%d); int32 wraparound?\n",
	      frmno, besthmmscr);
    }

    kb->hmm_hist[frm_nhmm / kb->hmm_hist_binsize]++;
    
    /* Set pruning threshold depending on whether number of active HMMs 
     * is within limit 
     */
    if (frm_nhmm > (maxhmmpf + (maxhmmpf >> 1))) {
      int32 *bin, nbin, bw;
      
      /* Use histogram pruning */
      nbin = 1000;
      bw = -(kb->beam->hmm) / nbin;
      bin = (int32 *) ckd_calloc (nbin, sizeof(int32));
      
      for (i = 0; i < (kb->n_lextree <<1); i++) {
	lextree = (i < kb->n_lextree) ?
	  kb->ugtree[i] : kb->fillertree[i - kb->n_lextree];
	
	lextree_hmm_histbin (lextree, besthmmscr, bin, nbin, bw);
      }
      
      for (i = 0, j = 0; (i < nbin) && (j < maxhmmpf); i++, j += bin[i]);
      ckd_free ((void *) bin);
      
      /* Determine hmm, phone, word beams */
      hb = -(i * bw);
      pb = (hb > kb->beam->ptrans) ? hb : kb->beam->ptrans;
      wb = (hb > kb->beam->word) ? hb : kb->beam->word;
    } else {
      hb = kb->beam->hmm;
      pb = kb->beam->ptrans;
      wb = kb->beam->word;
    }
    
    kb->bestscore = besthmmscr;
    kb->bestwordscore = bestwordscr;
    th = kb->bestscore + hb;	/* HMM survival threshold */
    pth = kb->bestscore + pb;	/* Cross-HMM transition threshold */
    wth = kb->bestwordscore + wb;	/* Word exit threshold */
    
    /*
     * For each lextree, determine if the active HMMs remain active for next
     * frame, propagate scores across HMM boundaries, and note word exits.
     */

    for (i = 0; i < (kb->n_lextree <<1); i++) {
      lextree = (i < kb->n_lextree) ? kb->ugtree[i] : 
	kb->fillertree[i - kb->n_lextree];

      /* Hack! Use narrow phone transition beam (wth) every few frames */
      if ((ptranskip < 1) || ((frmno % ptranskip) != 0))
	lextree_hmm_propagate(lextree, kbcore, kb->vithist, frmno,
			      th, pth, wth);
      else
	lextree_hmm_propagate(lextree, kbcore, kb->vithist, frmno,
			      th, wth, wth);
    }

    /* Limit vithist entries created this frame to specified max */
    vithist_prune (kb->vithist, dict, frmno, maxwpf, maxhistpf, wb);
    
    /* Cross-word transitions */
    utt_word_trans (kb, frmno);
    
    /* Wind up this frame */
    vithist_frame_windup (kb->vithist, frmno, NULL, kbcore);
    
    kb_lextree_active_swap (kb);

    ptmr_stop (&(kb->tm_srch));
  }

  
  kb->utt_hmm_eval += n_hmm_eval;
  kb->nfr += block_nfeatvec;
  
  *curfrm = frmno;
}

