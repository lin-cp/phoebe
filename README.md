![Build and Test](https://github.com/mir-group/phoebe/workflows/Build%20and%20Test/badge.svg)

# Phoebe <img src="doc/sphinx/source/_static/icon.png" width="25"/>

### A collection of Phonon and Electron Boltzmann Equation solvers

Phoebe is an open-source code for the ab-initio computation of electron and phonon transport properties of crystalline materials.

It is designed to take advantage of HPC systems via MPI-OpenMP hybrid parallelism, memory-distributed computing via ScaLAPACK, and GPU accelerated calculation of scattering rates.

For more details, see:

* Phoebe: a collection of Phonon and Electron Boltzmann Equation solvers.  
  A. Cepellotti, J. Coulter, A. Johansson, N. S. Fedorova, B. Kozinsky.  
  [arXiv:2111.14999](https://arxiv.org/abs/2111.14999). (2021).

Tutorials, documentation of functionality and underlying theory can be found at:
  * [Homepage](https://mir-group.github.io/phoebe/)
  * [Tutorials/Documentation](https://phoebe.readthedocs.io/en/develop/introduction.html)

For further questions or issues, please post on the discussions page for the git repo.

-------------------------
### Current functionalities
#### Electronic Transport

   * Electron-phonon scattering by Wannier interpolation
   * Electron-phonon scattering within the electron-phonon averaged (EPA) approximation
   * Polar correction and boundary scattering contributions to transport
   * Electronic transport coefficients (mobility, conductivity, thermal conductivity, and Seebeck coefficient)

#### Phonon Transport

   * 3-phonon scattering from thirdOrder.py/ShengBTE or Phono3py force constants
   * Boundary and isotope scattering contributions to transport
   * Phonon (lattice) thermal conductivity

#### And more...

   * BTE solutions by RTA, iterative, variational, and relaxons solvers
   * Calculation of electron and phonon linewidths or relaxation times on a path
   * Wigner transport equation correction for electrons and phonons (Zener tunneling contribution to electron transport)
   * Hydrodynamic transport properties (viscosity) for electrons and phonons
