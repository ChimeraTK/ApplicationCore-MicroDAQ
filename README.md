# ApplicationCore-MicroDAQ

The ApplicationCore-MicroDAQ package provides ApplicationCore modules for data acquisition.
It includes an abstract base class `ChimeraTK::BaseDAQ`, that can be used to implement different DAQ backends.
Currently two DAQ backends are implemented:

* `ChimeraTK::HDF5DAQ`: HDF5 based DAQ that uses uncompressed HDF5 files.
* `ChimeraTK::ROOTDAQ`: ROOT based DAQ that uses compressed ROOT files.

The provided modules include configuration and status process variables. DAQ problems are indicated by the process variable `DAQError`. 
In case of DAQ errors just disable and reenable the DAQ once.

The idea to use ROOT files instead of HDF5 files is to reduce the file size and improve the analysis performance, especially when working with many large files.  

## Envelope class

The envelope class `MicroDAQ` can used to include the DAQ into a server, while allowing to configure the DAQ via the server config file. 
In the config file, the following variables are required:

* MicroDAQ/enable (int32): boolean flag whether the MicroDAQ system is enabled or not
* MicroDAQ/outputFormat (string): format of the output data, either "hdf5" or "root"
* MicroDAQ/decimationFactor (uint32): decimation factor applied to large arrays (above decimationThreshold)
* MicroDAQ/decimationThreshold (uint32): array size threshold above which the decimationFactor is applied

If `MicroDAQ/enable == 0`, all other variables can be omitted.

In order to use the envelope class the `MicroDAQ` class needs to be defined after the `ChimeraTK::ConfigReader` in the server application. The `MicroDAQ` constructor takes an `inputTag`, which is used to identify
variables of other modules that should be connected to the DAQ module. The `tags` passed in the constructor of `MicroDAQ` will be added to all process variables of the 
DAQ. 

## Remark on DeviceModules

It is possible to connect DeviceModules to the DAQ. This can be done by calling `addDeviceModule` in the constructor of your application. 
In addition, it is possible to use the LogicalNameMapping backend to assign the `tag` used by the DAQ. See also the [tag modifier plugin](https://chimeratk.github.io/ChimeraTK-DeviceAccess/head/html/lmap.html#plugins_reference_tag_modifier). This allows to select individual variables from the device for the DAQ. 
In this case it is not necessary to call `addDeviceModule`.

## Remark on data types

The data stored in the *.h5 files is always of type `float`. In case of the ROOT backend the ChimeraTK data types are properly mapped to ROOT data types, which further reduces the file size and improves analysis performance.

## Remark on ROOT dictionary

It might happen that some includes are not found by ROOT. In that case setting the environment variable `ROOT_INCLUDE_PATH=/usr/` might help, in case an error is saying that `include/data_types.h` is not found.
