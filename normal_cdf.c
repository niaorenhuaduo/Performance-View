//
//  normal_cdf.c
//  Performance View
//
//  Created by Sanna Wager on 8/2/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#include "normal_cdf.h"


// Include file for tabulated normal distribution values
// -----------------------------------

// Tabulated table values(x) that has been for x<=0


float cdf[310];

int cdf_hash(float z) {
    return (int) fabsf(100 * z);
}

int cdf_create_table() {
    for (int i = 0; i < 310; i++) {
        int key = cdf_hash(cdf_values[i][0]);
        cdf[key] = cdf_values[i][1];
        printf("\n%d\t%f", key, cdf[key]);
    }
}

float cdf_gauss(float z) {
    int key = cdf_hash(z);
    if (z <= 0)
        return cdf[key];
    else return 1.0 - cdf[key];
}

float cdf_values[310][2] = {
    
    {-3.00, 0.0013},
    {-3.01, 0.0013},
    {-3.02, 0.0013},
    {-3.03, 0.0012},
    {-3.04, 0.0012},
    {-3.05, 0.0011},
    {-3.06, 0.0011},
    {-3.07, 0.0011},
    {-3.08, 0.001},
    {-3.09, 0.001},
    
    {-2.90, 0.0019},
    {-2.91, 0.0018},
    {-2.92, 0.0017},
    {-2.93, 0.0017},
    {-2.94, 0.0016},
    {-2.95, 0.0016},
    {-2.96, 0.0015},
    {-2.97, 0.0015},
    {-2.98, 0.0014},
    {-2.99, 0.0014},
    
    {-2.80, 0.0026},
    {-2.81, 0.0025},
    {-2.82, 0.0024},
    {-2.83, 0.0023},
    {-2.84, 0.0023},
    {-2.85, 0.0022},
    {-2.86, 0.0021},
    {-2.87, 0.0021},
    {-2.88, 0.002},
    {-2.89, 0.0019},
    
    {-2.70, 0.0035},
    {-2.71, 0.0034},
    {-2.72, 0.0033},
    {-2.73, 0.0032},
    {-2.74, 0.0031},
    {-2.75, 0.003},
    {-2.76, 0.0029},
    {-2.77, 0.0028},
    {-2.78, 0.0027},
    {-2.79, 0.0026},
    
    {-2.60, 0.0047},
    {-2.61, 0.0045},
    {-2.62, 0.0044},
    {-2.63, 0.0043},
    {-2.64, 0.0041},
    {-2.65, 0.004},
    {-2.66, 0.0039},
    {-2.67, 0.0038},
    {-2.68, 0.0037},
    {-2.69, 0.0036},
    
    {-2.50, 0.0062},
    {-2.51, 0.006},
    {-2.52, 0.0059},
    {-2.53, 0.0057},
    {-2.54, 0.0055},
    {-2.55, 0.0054},
    {-2.56, 0.0052},
    {-2.57, 0.0051},
    {-2.58, 0.0049},
    {-2.59, 0.0048},
    
    {-2.40, 0.0082},
    {-2.41, 0.008},
    {-2.42, 0.0078},
    {-2.43, 0.0075},
    {-2.44, 0.0073},
    {-2.45, 0.0071},
    {-2.46, 0.0069},
    {-2.47, 0.0068},
    {-2.48, 0.0066},
    {-2.49, 0.0064},
    
    {-2.30, 0.0107},
    {-2.31, 0.0104},
    {-2.32, 0.0102},
    {-2.33, 0.0099},
    {-2.34, 0.0096},
    {-2.35, 0.0094},
    {-2.36, 0.0091},
    {-2.37, 0.0089},
    {-2.38, 0.0087},
    {-2.39, 0.0084},
    
    {-2.20, 0.0139},
    {-2.21, 0.0136},
    {-2.22, 0.0132},
    {-2.23, 0.0129},
    {-2.24, 0.0125},
    {-2.25, 0.0122},
    {-2.26, 0.0119},
    {-2.27, 0.0116},
    {-2.28, 0.0113},
    {-2.29, 0.011},
    
    {-2.10, 0.0179},
    {-2.11, 0.0174},
    {-2.12, 0.017},
    {-2.13, 0.0166},
    {-2.14, 0.0162},
    {-2.15, 0.0158},
    {-2.16, 0.0154},
    {-2.17, 0.015},
    {-2.18, 0.0146},
    {-2.19, 0.0143},
    
    {-2.00, 0.0227},
    {-2.01, 0.0222},
    {-2.02, 0.0217},
    {-2.03, 0.0212},
    {-2.04, 0.0207},
    {-2.05, 0.0202},
    {-2.06, 0.0197},
    {-2.07, 0.0192},
    {-2.08, 0.0188},
    {-2.09, 0.0183},
    
    {-1.90, 0.0287},
    {-1.91, 0.0281},
    {-1.92, 0.0274},
    {-1.93, 0.0268},
    {-1.94, 0.0262},
    {-1.95, 0.0256},
    {-1.96, 0.025},
    {-1.97, 0.0244},
    {-1.98, 0.0239},
    {-1.99, 0.0233},
    
    {-1.80, 0.0359},
    {-1.81, 0.0351},
    {-1.82, 0.0344},
    {-1.83, 0.0336},
    {-1.84, 0.0329},
    {-1.85, 0.0322},
    {-1.86, 0.0314},
    {-1.87, 0.0307},
    {-1.88, 0.0301},
    {-1.89, 0.0294},
    
    {-1.70, 0.0446},
    {-1.71, 0.0436},
    {-1.72, 0.0427},
    
    {-1.73, 0.0418},
    {-1.74, 0.0409},
    {-1.75, 0.0401},
    {-1.76, 0.0392},
    {-1.77, 0.0384},
    {-1.78, 0.0375},
    {-1.79, 0.0367},
    
    {-1.60, 0.0548},
    {-1.61, 0.0537},
    {-1.62, 0.0526},
    {-1.63, 0.0516},
    {-1.64, 0.0505},
    {-1.65, 0.0495},
    {-1.66, 0.0485},
    {-1.67, 0.0475},
    {-1.68, 0.0465},
    {-1.69, 0.0455},
    
    {-1.50, 0.0668},
    {-1.51, 0.0655},
    {-1.52, 0.0643},
    {-1.53, 0.063},
    {-1.54, 0.0618},
    {-1.55, 0.0606},
    {-1.56, 0.0594},
    {-1.57, 0.0582},
    {-1.58, 0.0571},
    {-1.59, 0.0559},
    
    {-1.40, 0.0808},
    {-1.41, 0.0793},
    {-1.42, 0.0778},
    {-1.43, 0.0764},
    {-1.44, 0.0749},
    {-1.45, 0.0735},
    {-1.46, 0.0721},
    {-1.47, 0.0708},
    {-1.48, 0.0694},
    {-1.49, 0.0681},
    
    {-1.30, 0.0968},
    {-1.31, 0.0951},
    {-1.32, 0.0934},
    {-1.33, 0.0918},
    {-1.34, 0.0901},
    {-1.35, 0.0885},
    {-1.36, 0.0869},
    {-1.37, 0.0853},
    {-1.38, 0.0838},
    {-1.39, 0.0823},
    
    {-1.20, 0.1151},
    {-1.21, 0.1131},
    {-1.22, 0.1112},
    {-1.23, 0.1093},
    {-1.24, 0.1075},
    {-1.25, 0.1056},
    {-1.26, 0.1038},
    {-1.27, 0.102},
    {-1.28, 0.1003},
    {-1.29, 0.0985},
    
    {-1.10, 0.1357},
    {-1.11, 0.1335},
    {-1.12, 0.1314},
    {-1.13, 0.1292},
    {-1.14, 0.1271},
    {-1.15, 0.1251},
    {-1.16, 0.123},
    {-1.17, 0.121},
    {-1.18, 0.119},
    {-1.19, 0.117},
    
    {-1.00, 0.1587},
    {-1.01, 0.1562},
    {-1.02, 0.1539},
    {-1.03, 0.1515},
    {-1.04, 0.1492},
    {-1.05, 0.1469},
    {-1.06, 0.1446},
    {-1.07, 0.1423},
    {-1.08, 0.1401},
    {-1.09, 0.1379},
    
    {-0.90, 0.1841},
    {-0.91, 0.1814},
    {-0.92, 0.1788},
    {-0.93, 0.1762},
    {-0.94, 0.1736},
    {-0.95, 0.1711},
    {-0.96, 0.1685},
    {-0.97, 0.166},
    {-0.98, 0.1635},
    {-0.99, 0.1611},
    
    {-0.80, 0.2119},
    {-0.81, 0.209},
    {-0.82, 0.2061},
    {-0.83, 0.2033},
    {-0.84, 0.2005},
    {-0.85, 0.1977},
    {-0.86, 0.1949},
    {-0.87, 0.1922},
    {-0.88, 0.1894},
    {-0.89, 0.1867},
    
    {-0.70, 0.242},
    {-0.71, 0.2389},
    {-0.72, 0.2358},
    {-0.73, 0.2327},
    {-0.74, 0.2296},
    {-0.75, 0.2266},
    {-0.76, 0.2236},
    {-0.77, 0.2206},
    {-0.78, 0.2177},
    {-0.79, 0.2148},
    
    {-0.60, 0.2743},
    {-0.61, 0.2709},
    {-0.62, 0.2676},
    {-0.63, 0.2643},
    {-0.64, 0.2611},
    {-0.65, 0.2578},
    {-0.66, 0.2546},
    {-0.67, 0.2514},
    {-0.68, 0.2483},
    {-0.69, 0.2451},
    
    {-0.50, 0.3085},
    {-0.51, 0.305},
    {-0.52, 0.3015},
    {-0.53, 0.2981},
    {-0.54, 0.2946},
    {-0.55, 0.2912},
    {-0.56, 0.2877},
    {-0.57, 0.2843},
    {-0.58, 0.281},
    {-0.59, 0.2776},
    
    {-0.40, 0.3446},
    {-0.41, 0.3409},
    {-0.42, 0.3372},
    {-0.43, 0.3336},
    {-0.44, 0.33},
    {-0.45, 0.3264},
    {-0.46, 0.3228},
    {-0.47, 0.3192},
    {-0.48, 0.3156},
    {-0.49, 0.3121},
    
    {-0.30, 0.3821},
    {-0.31, 0.3783},
    {-0.32, 0.3745},
    {-0.33, 0.3707},
    {-0.34, 0.3669},
    {-0.35, 0.3632},
    {-0.36, 0.3594},
    {-0.37, 0.3557},
    {-0.38, 0.352},
    {-0.39, 0.3483},
    
    {-0.20, 0.4207},
    {-0.21, 0.4168},
    {-0.22, 0.4129},
    {-0.23, 0.409},
    {-0.24, 0.4052},
    {-0.25, 0.4013},
    {-0.26, 0.3974},
    {-0.27, 0.3936},
    {-0.28, 0.3897},
    {-0.29, 0.3859},
    
    {-0.10, 0.4602},
    {-0.11, 0.4562},
    {-0.12, 0.4522},
    {-0.13, 0.4483},
    {-0.14, 0.4443},
    {-0.15, 0.4404},
    {-0.16, 0.4364},
    {-0.17, 0.4325},
    {-0.18, 0.4286},
    {-0.19, 0.4247},
    
    {-0.00, 0.5},
    {-0.01, 0.496},
    {-0.02, 0.492},
    {-0.03, 0.488},
    {-0.04, 0.484},
    {-0.05, 0.4801},
    {-0.06, 0.4761},
    {-0.07, 0.4721},
    {-0.08, 0.4681},
    {-0.09, 0.4641}
    
};