//
//  normal_cdf.h
//  Performance View
//
//  Created by Sanna Wager on 8/2/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#ifndef normal_cdf_h
#define normal_cdf_h

#include <stdio.h>

#endif /* normal_cdf_h */

extern float cdf[310];
extern float cdf_values[310][2];

int cdf_create_table();
int cdf_hash(float z);
float cdf_gauss(float z);

