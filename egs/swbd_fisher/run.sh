#!/bin/bash

# this script generates Pocolm-estimated language models with combined data sources
# from swbd1 and fisher. We aim to compare pocolm's method of combining data
# from multiple sources with SRILM's (check /local/pocolm_with_srilm_combination.sh).

set -e
export POCOLM_ROOT=$(cd ../..; pwd -P)
export PATH=$PATH:$POCOLM_ROOT/scripts

local/swbd_data_prep.sh

# you have to have the Fisher corpus from LDC.
fisher_dirs="/export/corpora3/LDC/LDC2004T19/fe_03_p1_tran/ /export/corpora3/LDC/LDC2005T19/fe_03_p2_tran/"

local/fisher_data_prep.sh $fisher_dirs

num_word=40000
lm_dir="data/lm"
arpa_dir="data/arpa"

fold_dev_opt=
# If you want to fold the dev-set in to the 'swbd1' set to produce the final
# model, un-comment the following line.  For use in the Kaldi example script for
# ASR, this isn't suitable because the 'dev' set is the first 10k lines of the
# switchboard data, which we also use as dev data for speech recognition
# purposes.
#fold_dev_opt="--fold-dev-into=swbd1"

bypass_metaparam_optim_opt=
# If you want to bypass the metaparameter optimization steps with specific metaparameters
# un-comment the following line, and change the numbers to some appropriate values.
# You can find the values from output log of train_lm.py.
# These example numbers of metaparameters is for 3-gram model running with train_lm.py.
# the dev perplexity should be close to the non-bypassed model.
#bypass_metaparam_optim_opt="--bypass-metaparameter-optimization=0.500,0.763,0.379,0.218,0.034,0.911,0.510,0.376,0.127"
# Note: to use these example parameters, you may need to remove the .done files
# to make sure the make_lm_dir.py be called and tain only 3-gram model
#for order in 3; do
#rm -f ${lm_dir}/${num_word}_${order}.pocolm/.done


for order in 3 4 5; do
  # decide on the vocabulary.
  # Note: you'd use --wordlist if you had a previously determined word-list
  # that you wanted to use.
  # Note: the following might be a more reasonable setting:
  # train_lm.py --num-word=${num_word} --num-splits=5 --warm-start-ratio=10 \
  #             --min-counts='fisher=2 swbd1=1' \
  #             --keep-int-data=true ${fold_dev_opt} ${bypass_metaparam_optim_opt} \
  #             data/text ${order} ${lm_dir}
  train_lm.py --num-word=${num_word} --num-splits=5 --warm-start-ratio=10 \
              --keep-int-data=true ${fold_dev_opt} ${bypass_metaparam_optim_opt} \
              data/text ${order} ${lm_dir}
  unpruned_lm_dir=${lm_dir}/${num_word}_${order}.pocolm

  mkdir -p ${arpa_dir}
  format_arpa_lm.py ${unpruned_lm_dir} | gzip -c > ${arpa_dir}/${num_word}_${order}gram_unpruned.arpa.gz

  # example of pruning.  note: the threshold can be less than or more than one.
  get_data_prob.py data/text/dev.txt ${unpruned_lm_dir} 2>&1 | grep -F '[perplexity'
  for threshold in 1.0 2.0 4.0; do
    pruned_lm_dir=${lm_dir}/${num_word}_${order}_prune${threshold}.pocolm
    prune_lm_dir.py --final-threshold=${threshold} ${unpruned_lm_dir} ${pruned_lm_dir} 2>&1 | tail -n 5 | head -n 3
    get_data_prob.py data/text/dev.txt ${pruned_lm_dir} 2>&1 | grep -F '[perplexity'

    format_arpa_lm.py ${pruned_lm_dir} | gzip -c > data/arpa/${num_word}_${order}gram_prune${threshold}.arpa.gz

  done

  # example of pruning by size.
  size=250000
  pruned_lm_dir=${lm_dir}/${num_word}_${order}_prune${size}.pocolm
  prune_lm_dir.py --target-num-ngrams=${size} ${unpruned_lm_dir} ${pruned_lm_dir} 2>&1 | tail -n 8 | head -n 6 | grep -v 'log-prob changes'
  get_data_prob.py data/text/dev.txt ${pruned_lm_dir} 2>&1 | grep -F '[perplexity'

  format_arpa_lm.py ${pruned_lm_dir} | gzip -c > data/arpa/${num_word}_${order}gram_prune${size}.arpa.gz

done

# (run local/srilm_baseline.sh ${num_word} to see the following result e.g. local/srilm_baseline.sh 40000 )

# the following does does some self-testing, including
# that the computed derivatives are accurate.
# local/self_test.sh

# perplexities from pocolm-estimated language models with pocolm's interpolation
# method from orders 3, 4, and 5 are:
# order 3: optimize_metaparameters.py: final perplexity without barrier function was -4.358818 (perplexity: 78.164689)
# order 4: optimize_metaparameters.py: final perplexity without barrier function was -4.309507 (perplexity: 74.403797)
# order 5: optimize_metaparameters.py: final perplexity without barrier function was -4.301741 (perplexity: 73.828181)

# note, the perplexities from pocolm-estimated language models with SRILM's
# interpolation from orders 3 and 4 are (from local/pocolm_with_srilm_combination.sh),
# 78.8449 and 75.2202 respectively.

# note, the perplexities from SRILM-estimated language models with SRILM's
# interpolation tool from orders 3 and 4 are (from local/srilm_baseline.sh),
# 78.9056 and 75.5528 respectively.
