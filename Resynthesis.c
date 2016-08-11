//
//  Resynthesis.c
//  Performance View
//
//  Created by Sanna Wager on 6/27/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#include "Resynthesis.h"
#include "global.h"
#include "polifitgsl.h"

bool polynomialfit(int obs, int degree,
                   double *dx, double *dy, double *store) /* n, p */
{
    gsl_multifit_linear_workspace *ws;
    gsl_matrix *cov, *X;
    gsl_vector *y, *c;
    double chisq;
    
    int i, j;
    
    X = gsl_matrix_alloc(obs, degree);
    y = gsl_vector_alloc(obs);
    c = gsl_vector_alloc(degree);
    cov = gsl_matrix_alloc(degree, degree);
    
    for(i=0; i < obs; i++) {
        for(j=0; j < degree; j++) {
            gsl_matrix_set(X, i, j, pow(dx[i], j));
        }
        gsl_vector_set(y, i, dy[i]);
    }
    
    ws = gsl_multifit_linear_alloc(obs, degree);
    gsl_multifit_linear(X, y, c, cov, &chisq, ws);
    
    /* store result ... */
    for(i=0; i < degree; i++)
    {
        store[i] = gsl_vector_get(c, i);
    }
    
    gsl_multifit_linear_free(ws);
    gsl_matrix_free(X);
    gsl_matrix_free(cov);
    gsl_vector_free(y);
    gsl_vector_free(c);
    return true; /* we do not "analyse" the result (cov matrix mainly)
                  to know if the fit is "good" */
}

void init_feature_list(AUDIO_FEATURE_LIST *a) {
    a->mu = a->var = a->sd = 0;
    a->num = 0;
    memset(a->amplitude.mu, 0, sizeof(double)*128);
    memset(a->amplitude.sd, 0, sizeof(double)*128);
    memset(a->amplitude.var, 0, sizeof(double)*128);
    //memset(a->amplitude.musq, 0, sizeof(double)*128);
    //memset(a->amplitude.num, 0, sizeof(int)*128);
}

//void add_amplitude_elem(AUDIO_FEATURE_LIST *list, int nominal, float amp) {
//    list->amplitude.num[nominal]++;
//    double temp1 = (double) logf(amp);
//    double temp2 = (double) powf(logf(amp), 2);
//    list->amplitude.mu[nominal] += (double) logf(amp);
//    list->amplitude.musq[nominal] += (double) powf(logf(amp), 2);
//}

//void cal_amplitude_dist(AUDIO_FEATURE_LIST *list) {
//    for (int i = 0; i < 128; i++) {
//        if (list->amplitude.num[i] == 0) {
//            list->amplitude.var[i] = list->amplitude.sd[i] = 1;
//        }
//        else {
//            list->amplitude.mu[i] /= (double) list->amplitude.num[i];
//            list->amplitude.musq[i] /= (double) list->amplitude.num[i];
//            list->amplitude.var[i] = list->amplitude.musq[i] - pow(list->amplitude.mu[i], 2);
//            list->amplitude.sd[i] = sqrt(list->amplitude.var[i]);
//        }
//    }
//}

void cal_amplitude_dist(AUDIO_FEATURE_LIST *list, int frames) {
    int polydegree = 3, ind = 0;
    double *amp = (double*) malloc (frames * sizeof(double));
    double *nom = (double*) malloc (frames * sizeof(double));

    /* copy the values into arrays of doubles */
    for (int i = 0; i < list->num; i++){
        if (list->el[i].amp < 0.001) continue; //remove silent frames
        amp[ind] = (double) logf(list->el[i].amp);
        nom[ind] = (double) list->el[i].nominal;
        ind++;
    }

    /* Ordinary Least Squares regression */
    double *coeff = (double*) malloc (polydegree * sizeof(double));
    polynomialfit(ind, polydegree, nom, amp, coeff);
    
    /* calculate estimate of amplitude mean for each MIDI index */
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < polydegree; j++) {
            list->amplitude.mu[i] += coeff[j] * pow(i, j);
        }
        printf("\namp mu %lf", list->amplitude.mu[i]);
    }

    /* calculate squared deviation from the mean using OLS */
    for (int i = 0; i < frames; i++) {
        int temp1 = nom[i];
        amp[i] -= list->amplitude.mu[(int)nom[i]];
        amp[i] = pow(amp[i],2);
    }
    
    polynomialfit(ind, polydegree, nom, amp, coeff);
    
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < polydegree; j++) {
            list->amplitude.sd[i] += coeff[j] * pow(i, j);
        }
        list->amplitude.sd[i] = max(0.0001, list->amplitude.sd[i]); //remove negatives
        /* get standard deviation from variance */
        list->amplitude.sd[i] = sqrt(list->amplitude.sd[i]);
        printf("\nsd %lf", list->amplitude.sd[i]);

    }
    
    free(nom);
    free(amp);
    free(coeff);
}

int binary_search(int firstnote, int lastnote, int search)
//finds which midi pitch the search frame belongs to
{
    int c, first, last, middle;
    
    first = firstnote;
    last = lastnote;
    middle = (first+last)/2;
    
    while (first < last) {
        if (score.solo.note[middle].frames > search) //frame occured before the beginning of the middle note
            last = middle;
        else if (score.solo.note[middle+1].frames > search) { //found note
            if (is_solo_rest(middle) == 1) {
                return(-2); //note is a rest
            }
            return score.solo.note[middle].snd_notes.snd_nums[0]; //return the midi pitch
        }
        else //frame occurred later than the current note
            first = middle + 1;
        middle = (first + last)/2;
    }
    
    return(-1);
}


//audiodata_target
void prep_cal_feature(int frame, unsigned char* audioname) {
    int offset, first, last, midi;
    unsigned char* ptr;
    float hz0;
    if (!(audioname != audiodata || audioname != audiodata_target)) { printf("prep_cal_feature argument is audiodata or audiodata_target"); exit(0); }
    offset = frame*SKIPLEN*BYTES_PER_SAMPLE;
    
    if (audioname == audiodata) {
        first = firstnote;
        last = lastnote;
    } else {
        //first = firstnote_target; //need to create these
        //last = lastnote_target;
    }
    
    ptr = audioname + offset;
    int temp = score.solo.note[first].frames;
    if (frame < score.solo.note[first].frames || frame > score.solo.note[last].frames) { printf("prep_cal_feature frame index out of score bounds"); exit(0); }
    midi = binary_search(first, last, frame);
    hz0 = (int) (pow(2,((midi - 69)/12.0)) * 440);
    
    //cal_feature(ptr, hz0);
}

//dp: do not allow jump when source is changing note. must use entire change of note


