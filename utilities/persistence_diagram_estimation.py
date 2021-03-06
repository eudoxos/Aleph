#!/usr/bin/env python3
#
# This file is part of 'Aleph - A Library for Exploring Persistent
# Homology'. It contains code for performing *intensity estimates*
# of persistence diagrams.
#
# Original author: Bastian Rieck

import math
import numpy
import sys

import matplotlib.pyplot as plt
import scipy.stats       as stats

def point_estimates(samples):
  mean  = numpy.mean(samples)
  var   = numpy.var(samples)
  alpha = mean * mean / var
  beta  = mean / var

  return alpha,beta

def pdf(x, alpha, beta):
  return stats.gamma.pdf(x, a=alpha, scale=1.0/beta)

def likelihood(data, hypothesis):
  c,d                              = data
  c_alpha, c_beta, d_alpha, d_beta = hypothesis

  likelihood_c = pdf(c, c_alpha, c_beta)
  likelihood_d = pdf(d, d_alpha, d_beta)

  return likelihood_c * likelihood_d

def likelihood_single(data, hypothesis):
  c           = data
  alpha, beta = hypothesis

  return pdf(c, alpha, beta)

def log_likelihood_multiple(data, hypothesis):
  l           = 0.0
  alpha, beta = hypothesis

  for x in data:
    l += stats.gamma.logpdf(x, a=alpha, scale=1.0/beta)

  return l

def max_likelihood(posteriors):
  max_likelihood = -sys.float_info.max
  max_arg        = (0,0)

  for (alpha,beta) in sorted( posteriors.keys() ):
    likelihood = posteriors[ (alpha,beta) ]
    if likelihood > max_likelihood:
      max_likelihood = likelihood
      max_arg        = (alpha,beta)

  return max_likelihood, max_arg

def make_prior_ranges(alpha, beta, n, N, k=3):
  mean    = alpha/beta
  var     = alpha/beta**2
  se_mean = mean / math.sqrt(n)
  se_var  = math.sqrt(2*var**2 / (n-1))

  means   = numpy.linspace(mean - se_mean * k, mean + se_mean * k, N)
  vars    = numpy.linspace(var - se_var * k, var + se_var * k, N)

  betas    = [ mean / var for mean,sigma in zip(means,vars) ]
  alphas   = [ mean**2 / var for mean,sigma in zip(means,vars) ]

  return alphas, betas

if __name__ == "__main__":
  filename = sys.argv[1]
  data     = list()

  with open(filename) as f:
    for line in f:
      if not line.strip() or line.startswith('#'):
        continue

      (c,d) = [ float(x) for x in line.strip().split() ]

      if not math.isinf(c) and not math.isinf(d):
        # FIXME: does it make sense to do the intensity-based
        # transformation here?
        #data.append( (c+d,d-c) )
        data.append( (c,d) )

  # FIXME: the point estimates do not yet work as expected, so I am
  # required to add these fake estimates here.
  c_alpha, c_beta = [7.0, 0.7] #point_estimates( [ c for c,_ in data ] )
  d_alpha, d_beta = [8.0, 1.9] #point_estimates( [ d for _,d in data ] )

  # Plot the distributions to ensure that the point estimates are
  # actually useful.

  if False:
    v = [ c for c,_ in data ]
    x = numpy.linspace(min(v), max(v), 100)

    plt.hist([c for c,_ in data], normed=True, bins=20, label="Creation [samples]")
    plt.plot(x, pdf(x, c_alpha, c_beta), label="Creation [estimate]")
    plt.legend()
    plt.show()

    v = [ d for _,d in data ]
    x = numpy.linspace(min(v), max(v), 100)

    plt.hist([c for c,_ in data], normed=True, bins=20, label="Destruction [samples]")
    plt.plot(x, pdf(x, d_alpha, d_beta), label="Destruction [estimate]")
    plt.legend()
    plt.show()

  c_alphas, c_betas = make_prior_ranges(c_alpha, c_beta, len(data), 10)
  d_alphas, d_betas = make_prior_ranges(d_alpha, d_beta, len(data), 10)

  c_posteriors = dict()
  d_posteriors = dict()

  for c_alpha in c_alphas:
    for c_beta in c_betas:
      c_posteriors[ (c_alpha, c_beta) ] = c_posteriors.get((c_alpha, c_beta), 1.0) + log_likelihood_multiple([c for c,_ in data[:100]], (c_alpha, c_beta))
      #for c in [ c for c,_ in data[:1000]]:
      #  c_posteriors[ (c_alpha, c_beta) ] = c_posteriors.get((c_alpha, c_beta), 1.0) * likelihood_single(c, (c_alpha, c_beta))

  c_posteriors.update( ((a,b), math.exp( c_posteriors[ (a,b) ] )) for a,b in c_posteriors )

  print("Finished posterior estimation for creation values")

  for d_alpha in d_alphas:
    for d_beta in d_betas:
      d_posteriors[ (d_alpha, d_beta) ] = d_posteriors.get((d_alpha, d_beta), 1.0) + log_likelihood_multiple([d for _,d in data[:100]], (d_alpha, d_beta))
      #for d in [ d for _,d in data[:1000]]:
      #  d_posteriors[ (d_alpha, d_beta) ] = d_posteriors.get((d_alpha, d_beta), 1.0) * likelihood_single(d, (d_alpha, d_beta))

  d_posteriors.update( ((a,b), math.exp( d_posteriors[ (a,b) ] )) for a,b in d_posteriors )

  print("Finished posterior estimation for destruction values")

  _, c_parameters = max_likelihood(c_posteriors)
  _, d_parameters = max_likelihood(d_posteriors)

  v = [ c for c,_ in data ]
  x = numpy.linspace(min(v), max(v), 100)

  #plt.hist([c for c,_ in data], normed=True, bins=20, label="Creation [samples]")
  #plt.plot(x, pdf(x, c_parameters[0], c_parameters[1]), label="Creation [Bayes]")
  #plt.plot(x, pdf(x, c_alpha, c_beta), label="Creation [estimate]")
  #plt.legend()
  #plt.show()

  xs = sorted([ c for c,_ in data[:100] ])
  ys = sorted([ d for _,d in data[:100] ])

  X, Y = numpy.meshgrid(xs, ys)
  f    = lambda x,y: likelihood( (x,y), (c_parameters[0], c_parameters[1], d_parameters[0], d_parameters[1]) )
  f    = numpy.vectorize(f)
  Z    = f(X,Y)

  plt.pcolormesh(X,Y,Z)
  plt.scatter([ c for c,_ in data[:100] ], [ d for _,d in data[:100] ] )

  plt.show()

  #plt.hist(numpy.exp( list( c_posteriors.values() ) ) )
  #plt.show()
