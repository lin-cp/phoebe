#!/usr/bin/env python3
import json
import matplotlib.pyplot as plt
import numpy as np
import argparse
import itertools
import os
import sys

# script to plot the EPA relaxation times found in epa_relaxation_times.json

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Plot relaxation times that "
                                     "have been generated by Phoebe using the EPA method.")
    parser.add_argument("INPUT",
                        help="Name of the EPA JSON file with relaxation times")
    parser.add_argument("calcIndex",
                        help="Index of temperature/chemical potential to plot",
                        default=0)
    args = parser.parse_args()

    # we select one calculation and check that the json file is correct
    try:
        calcIndex = int(args.calcIndex)
    except ValueError:
        raise ValueError("calcIndex should be an integer")

    # load in the json output
    jfileName = args.INPUT
    with open(jfileName) as jfile:
        data = json.load(jfile)

    try:
        data['relaxationTimes']
    except KeyError:
        raise KeyError("relaxation times not found."
              "Are you using the correct input json file?")

    particleType = data['particleType']

    # unpack the json file
    tau = np.array(data['relaxationTimes'])    # dimensions (iCalc, ik, ib)
    tau[np.where(tau==None)] = 0   # remove None values (from gamma pt acoustic ph)
    energies = np.array(data['energies'])      # dimensions (iCalc, ik, ib)
    linewidths = np.array(data['linewidths'])      # dimensions (iCalc, ik, ib)
    mu = np.array(data['chemicalPotentials'])
    T = np.array(data['temperatures'])

    linewidths = linewidths[calcIndex].flatten()
    energies = energies[calcIndex].flatten()
    tau = tau[calcIndex].flatten()
    mu = mu[calcIndex]
    energies = energies - mu

    print("Calculation Temperature: ", T[calcIndex])

    for y, name in [[tau,'tau'], [linewidths,'Gamma']]:

        # plot the lifetimes
        plt.figure(figsize=(5,5))

        plt.scatter(energies, y, marker='o', s=18, color='royalblue')

        # plot aesthetics
        plt.yscale('log')
        plt.xlabel(r'Energy [' + data['energyUnit'] +']',fontsize=12)
        if name == 'tau':
            units = ' [' + data['relaxationTimeUnit'] + ']'
        else:
            units = ' [' + data['linewidthsUnit'] + ']'

        plt.ylabel(r'$\{}_{{'.format(name) + data['particleType'] + '}$' +
                   units, fontsize=12)

        if (particleType=="phonon"):
            plt.xlim(0,None)

        # Find limits of the y axis
        zeroIndex = np.argwhere(y<=0.)
        y = np.delete(y, zeroIndex)
        ymin = 10**np.floor(np.log10(np.min(y)))
        ymax = 10**np.ceil(np.log10(np.max(y)))
        plt.ylim(ymin, ymax)

        plt.tight_layout()
        plt.ylim(0.1, 1000)
        plotFileName = os.path.splitext(jfileName)[0] + ".{}.png".format(name.lower())
        plt.savefig(plotFileName,dpi=150)
