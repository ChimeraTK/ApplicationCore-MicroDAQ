# ApplicationCore-MicroDAQ

The ApplicationCore-MicroDAQ package provides ApplicationCore modules for data aquisition.
It includes an abstract base class `ChimeraTK::BaseDAQ`, that can be used to implement different DAQ backends.
Currently two DAQ backends are implemented:

* `ChimeraTK::HDF5DAQ`: HDF5 based DAQ that uses uncompressed HDF5 files.
* `ChimeraTK::ROOTDAQ`: ROOT based DAQ that uses compressed ROOT files.

The provided modules include configuration and status process variables. DAQ problems are indicated by the process variable `DAQError`. 
In case of DAQ errors just disable and reenable the DAQ once.

The idea to use ROOT files instead of HDF5 files is to reduce the file size and improve the analysis performance, especially when working with many large files.  

## Envelope class

The envelope class `MicroDAQ` can used to include the DAQ into a server, while allowing to configure the daq via the server config file. 
In the config file, the following variables are required:

* MicroDAQ/enable (int32): boolean flag whether the MicroDAQ system is enabled or not
* MicroDAQ/outputFormat (string): format of the output data, either "hdf5" or "root"
* MicroDAQ/decimationFactor (uint32): decimation factor applied to large arrays (above decimationThreshold)
* MicroDAQ/decimationThreshold (uint32): array size threshold above which the decimationFactor is applied

If `MicroDAQ/enable == 0`, all other variables can be omitted.

In order to use the envelope class the `MicroDAQ` class needs to be defined after the `ChimeraTK::ConfigReader` in the server application. The `MicroDAQ` consturctor takes an `inputTag`, which is used to identify
variables of other modules that should be connected to the DAQ module. The `tags` passed in constructor of `MicroDAQ` will be added to all process variables of the 
daq. If you add e.g. `"CS"` you can connect all process variables of the daq to the control system by using a universal method call: 

      findTag("CS").connectTo(cs);
      


## Remark on DeviceModules

It is possible to connect DeviceModules to the DAQ. This requires to connect them to the ControlSystem fist and after connect the corresponing control system part to the 
daq using the `addSource` method. That is currently not possible using the envelope class, because it requires a certain tag to identify the daq variables and currently 
it is not possible to add tags to the DeviceModule. 

## Remark on data types

The data stored in the *.h5 files if always of tpye `float`. In case of the ROOT backend the ChimerTK data types are properly mapped to ROOT data types, which further reduces the file size and improoves analysis performance.

## Remark on ROOT dictionary

It might happen that some includes are not found by ROOT. In that case setting the environment variable `ROOT_INCLUDE_PATH=/usr/` might help, in case an error is saying that `include/data_types.h` is not found.