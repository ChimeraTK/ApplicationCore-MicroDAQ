# ApplicationCore-MicroDAQ

The ApplicationCore-MicroDAQ package provides ApplicationCore modules for data aquisition.
It includes an abstract base class `ChimeraTK::BaseDAQ`, that can be used to implement different DAQ backends.
Currently two DAQ backends are implemented:

* `ChimeraTK::HDF5DAQ`: HDF5 based DAQ that uses uncompressed HDF5 files.
* `ChimeraTK::ROOTDAQ`: ROOT based DAQ that uses compressed ROOT files.

The provided modules include configuration and status  process variables that can be connected to the control system using the following tags:

* `MicroDAQ.CONFIG`: Configuration of the DAQ system, like enable, buffer size, triggers per buffer
* `MicroDAQ.STATUS`: Error flag indicating problems of the DAQ, current DAQ path, current buffer and current DAQ entry

In case of DAQ errors just disable and reenable the DAQ once.


The idea to use ROOT files instead of HDF5 files is to reduce the file size and improve the analysis performance, especially when working with many large files.  

## Remark on data types

The data stored in the *.h5 files if always of tpye `float`. In case of the ROOT backend the ChimerTK data types are properly mapped to ROOT data types, which further reduces the file size and improoves analysis performance.
