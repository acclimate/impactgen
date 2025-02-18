# IMPACTGEN - a C++ tool for generating gridded (climate) impact data

## Introduction

IMPACTGEN enables the fast and flexible generation of regional-sectoral forcing data for Acclimate in NETCDF format based on gridded forcing data. 
Originally developed as a pre-processing tool for the model Acclimate [https://github.com/acclimate/acclimate], it enables the combination of climate data from one or multiple impact channels, flexible parametrisation of the impact functions, and use of proxy weighting schemes.

## Inputs

- gridded climate data as an input to the impact function
- gridded proxy data for weighting of the impacts (e.g. by gridded GDP, or other proxies of spatial economic activity)

- parametrisation data of the impact function, consisting of parameters for (sub-)regions, and a mapping of grid cells to these

- mapping of grid cells to larger regions for output files

## Available forcing types

The specific forcings are defined in the impacts subfolder. Available at the moment:

- Flooding.cpp - forcing based on innundation data from e.g. CAMA-flood (as used in Willner et. al 2018)
- HeatLaborProductivity.cpp - forcing based on Hsiang et al 2011 and used e.g. in Kuhla et al. 2021, 2022
- TropcialCyclones.cpp - used in Kuhla et. al 2021 for the impact of tropical cyclones
- AlphaBetaForcingPerSubregion: using subregional parameter regions and a general impact function of the type 
        
        $ \text{impact}(t) = \text{slope } \alpha \cdot \text{impact variable } I(t) + text{intercept } \beta $
- ExtremeTemperaturesPerSubregion: using subregional parameter regions for a extreme temperature forcing as used in Quante et. al 2024
        
        $ d(T_{daily_{r}}) = \alpha_{r,s} \max \left(0,\left(T_{daily_{r}}-T_{heat_{r,s}}\right)\right) ^2 + \beta_{r,s} \max \left(0,\left(T_{cold_{r,s}} - T_{daily_{r}}\right)\right) ^2 + \gamma_{r,s} $ 

- HeatedProductivity -- forcing on labor productivity (depending different worker types) due to wet-bulb-temperature. 

## Example header of a yml settings file

```
reference: *specification of timecoordinate start for netcdf*

combination: *specify how multiple impacts should be combined - by addition or multiplication*

impacts:
- type: *name of the impact routine to be used*
  chunk_size: 25
  isoraster:
    file: *mapping of grid impacts to regions using iso codes*
    variable: iso
  parameters_raster:
    file: *mapping of parameters to grid cells usning iso codes*
    variable: iso
  proxy:
    file: *proxy data as netcdf*
    variable: *variable name in the netcdf*
  forcing:
    *climate data needed for forcing function*
    variable: *variable name in the netcdf*

regions:
  type: netcdf
  file: *regions for impact output file*
  variable: region
sectors:
  type: netcdf
  file: *sectors for impact output file*
  variable: sector
output:
  file: *name of the output file*


(c) Sven Willner et. al @Potsdam-Institute for Climate Impact reserach

parameters:
    Region1: *regions as defined in parameters_raster*
        *parameters needed for forcing function*
```
