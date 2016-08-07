//
//  gauss_qt.cpp
//  Performance View
//
//  Created by Sanna Wager on 8/1/16.
//  Copyright © 2016 craphael. All rights reserved.
//

#include "gauss_qt.hpp"
#include "rng.hpp"
#include <math.h>
#include <string>
#include <stdio.h>
#include <stdint.h>
#include "mpreal.hpp"
#include <boost/math/distributions/normal.hpp>
using namespace mpfr;

// Get us a 256-bit quantile implementation
namespace MyQuantile {
    static const int kBits(256);
#include "quantile.hpp"
}

void t1()
{
    static const int nbSamples = 20;
    const boost::math::normal dist(0.0, 1.0);
    
    for(int i=0; i<=nbSamples; ++i) {
        
        double mu = i/(double)nbSamples;
        
        double y0 = 1e-12;
        double y1 = 1.0 - y0;
        double  y = y0 + mu*(y1 - y0);
        
        mpreal  x =  MyQuantile::quantile(y);
        mpreal yp =  MyQuantile::cdf(x);
        mpreal err =  fabs(yp - y);
        
        printf(
               "cdf(quantile(%.40f)) = %s    err= %s\n",
               y,
               yp.toString().c_str(),
               err.toString().c_str()
               );
        
        double q = quantile(dist, y);
        printf(
               "       myq = %s\n"
               "     boost = %.40f\n"
               "  cdf(myq) = %s\n"
               "cdf(boost) = %s\n"
               "\n",
               x.toString().c_str(),
               q,
               MyQuantile::cdf(x).toString().c_str(),
               MyQuantile::cdf(q).toString().c_str()
               );
    }
    printf("\n");
}



double qt_gauss(float z) {
    double d = MyQuantile::quantile((double)z).toDouble();
    return (float) d;
}

void test1() {
    t1();
}





