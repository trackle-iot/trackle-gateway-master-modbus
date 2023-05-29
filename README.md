# Trackle Gateway Master Modbus Component

This repository contains an ESP-IDF component that implements the functionality of accessing Modbus registers on connected slaves through typed virtual registers.

## Quick start

Basic usage of registers is as follows:
* Add a register using `AddRegister`;
* Configure its details using POST calls;
* Read its value using `ReadRegisterValue`;
* If set as writable, set its value using `WriteRegisterValue`;

Registers can be monitored:
* Enable monitoring using `MonitorRegister`;
* Set publishing interval using `SetRegisterMaxPublishDelay`;

Furthermore, registers can be monitored for change (register must be already monitored):
* Enable it using `EnableMonitorOnChange`;
* Set change check interval using `SetRegisterChangeCheckInterval`;
* Set modbus polling period using `SetMbReadPeriod`.

If Modbus slave address and register ID are known, one can read and write a register by calling `ReadRawRegisterValue` and `WriteRawRegisterValue` without adding them with `AddRegister`. Obviously, in this case, the read and write operations can be performed only with raw 16bit unsigned integers as values, since the type of the register's content is not known to the system.

To set Modbus details, methods `SetMb...` can be used. In order to make them effective, they must be saved to flash with `GwMasterModbus_saveConfigToFlash` and the device must restart.

Changes applied to registers are applied immediately, without the need to save to flash and restart.

`GwMasterModbus_saveConfigToFlash` also saves configuration about registers.

## Registers types

The following types are available for registers.

### number
Floating point number.

On read, the following steps are performed:
* Value is read via Modbus;
* It is converted to signed or unsigned integer;
* It is multiplied by a user defined factor;
* An user defined offset is added;
* The value is rounded to the Nth decimal (where N is defined by the user).

On write:
* The offset is subtracted;
* The value is divided by the factor;
* Value is converted to signed or unsigned integer;
* Value is written via Modbus.

### raw
Decimal unsigned 16 bit value contained in the register. During read and write, the value is used with no conversion.

### float
Floating point number stored in Modbus RTU registers by encoding it following the IEEE 754 standard.

### string
ASCII string saved in sequence of Modbus registers. Each Modbus register contains two characters in big-endian order.

## Methods
This section contains the description of the methods available from Trackle's cloud.

### POST
Methods available through POST calls.

#### AddRegister
* Description:
  * Add a register to volatile memory.
* Argument format:
  * `<name>,<readFunction>,<slaveAddr>,<regId>,<type>`
* Parameters:
  * `<name>`: name of the register to add.
  * `<readFunction>`: Modbus RTU read function code:
    * 1: Coils (FC=01)
    * 2: Discrete Inputs (FC=02)
    * 3: Multiple Holding Registers (FC=03)
    * 4: Input Registers (FC=04)
  * `<slaveAddr>`: Modbus RTU address of the slave owning the register.
  * `<regId>`: Modbus RTU register index on the slave.
  * `<type>`: type of the register, currently `raw` (for raw value of the register), `number` (if scaling or offsetting is required), `float` or `string`.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: empty name, or name too long;
  * -6: readFunction is not a valid Modbus read function;
  * -7: slaveAddr is not a valid 8bit unsigned integer;
  * -8: regId is not a valid 16bit unsigned integer;
  * -9: invalid type;
  * -10: name and/or (slaveAddr,regId) pair already used, or max number of registers reached.

#### DeleteRegister
* Description:
  * Delete a register from volatile memory.
* Argument format:
  * `<name>`
* Parameters:
  * `<name>`: name of the register to delete.
* Return values:
  * 1:  success;
  * -1: register name not found.

#### MonitorRegister
* Description:
  * Publish value read from a register periodically or on change of its value.
* Argument format:
  * `<name>,<bool>`
* Parameters:
  * `<name>`: name of the register to monitor.
  * `<bool>`: `true` to monitor register, `false` to stop monitoring.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: bool parameter is not a valid boolean value;
  * -6: register name not found.

#### SetRegisterLength
* Description:
  * Set number of Modbus RTU registers on slave that will hold the value of the specified register.
* Argument format:
  * `<name>,<length>`
* Parameters:
  * `<name>`: name of register.
  * `<length>`: number of Modbus registers that will hold the value.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: if specified length is not compatible with register type or register doesn't exist.

#### SetRegisterMaxPublishDelay
* Description:
  * Set time after which the register's value must be pusblished since last publish. If delay is set to 0, this kind of publish is disabled.
* Argument format:
  * `<name>,<seconds>`
* Parameters:
  * `<name>`: name of monitored register.
  * `<seconds>`: delay in seconds since last publish.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: seconds is not a valid 32bit unsigned integer;
  * -6: register name not found, or register is not monitored.

#### EnableMonitorOnChange
* Description:
  * For a register that is already being monitored, specify if monitoring must occur also on change of its value.
* Argument format:
  * `<name>,<bool>`
* Parameters:
  * `<name>`: name of the register to monitor on change.
  * `<bool>`: `true` to enable publishing on change, `false` to disable publishing on change.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: bool parameter is not a valid boolean value;
  * -6: register name not found, or register is not monitored.

#### SetRegisterChangeCheckInterval
* Description:
  * For a register that is already being monitored, and for which the publishing on change has been enabled, specify the period used to check if it's value has changed.
* Argument format:
  * `<name>,<seconds>`
* Parameters:
  * `<name>`: name of the monitored on change register.
  * `<seconds>`: number of seconds of the period.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: seconds is not a valid 32bit unsigned integer;
  * -6: register name not found, or register is not monitored on change.

#### SetMbReadPeriod
* Description:
  * Set minimum period used for reading monitored registers.
* Argument format:
  * `<seconds>`
* Parameters:
  * `<seconds>`: number of seconds of the period.
* Return values:
  * 1:  success;
  * -1: argument is not a valid period in seconds.

#### MakeRegisterWritable
* Description:
  * Make a register R/W or read-only.
* Argument format:
  * `<name>,<bool>,<writeFunction>`
* Parameters:
  * `<name>`: name of the register.
  * `<bool>`: `true` to enable writing to register, `false` to make it read-only.
  * `<writeFunction>`: Modbus RTU write function code (only if bool is true):
    * 5: Single Coil (FC=05)
    * 6: Single Holding Register (FC=06)
    * 15: Multiple Coils (FC=15)
    * 16: Multiple Holding Registers (FC=16)
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: bool parameter is not a valid boolean value;
  * -6: writeFunction is not a valid Modbus write function;
  * -7: writeFunction parameter not needed
  * -8: register name not found.

#### WriteRegisterValue
* Description:
  * Write typed value to writable register.
* Argument format:
  * `<name>,<value>`
* Parameters:
  * `<name>`: name of the register to write.
  * `<value>`: value compatible with type of the register.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: register name not found;
  * -6: Modbus RTU not initialized;
  * -7: register is not marked as writable;
  * -8: error while performing write command via Modbus RTU;
  * -9: value does not represent a valid double;
  * -10: raw value (obtained eventually after scaling/offsetting) doesn't fit in a 16 bit signed integer;
  * -11: raw value (obtained eventually after scaling/offsetting) doesn't fit in a 16 bit unsigned integer;
  * -12: internal error.

#### WriteRawRegisterValue
* Description:
  * Write 16 bit unsigned integer to register.
* Argument format:
  * `<writeFunction>,<slaveAddr>,<registerId>,<value>`
* Parameters:
  * `<writeFunction>`: Modbus RTU write function code:
    * 5: Single Coil (FC=05)
    * 6: Single Holding Register (FC=06)
    * 15: Multiple Coils (FC=15)
    * 16: Multiple Holding Registers (FC=16)
  * `<slaveAddr>`: address of the Modbus slave.
  * `<registerId>`: register where the value must be written.
  * `<value>`: 16 bit usigned integer to write to the register.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: writeFunction is not a valid Modbus write function;
  * -6: slaveAddr is not a valid 8bit unsigned integer;
  * -7: regId is not a valid 16bit unsigned integer;
  * -8: value is not a valid 16bit unsigned integer;
  * -9: Modbus not running;
  * -10: Error in modbus write;
  * -11: Internal error.

#### MakeRegisterSigned
* Description:
  * Interpret bits from a `number`-typed register as signed or unsigned.
* Argument format:
  * `<name>,<bool>`
* Parameters:
  * `<name>`: name of the `number`-typed register.
  * `<bool>`: `true` to interpret register as signed, `false` to make it unsigned.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: bool parameter is not a valid boolean value;
  * -6: register name not found.

#### SetRegisterCoefficients
* Description:
  * Set factor and offset for a `number`-typed register.
* Argument format:
  * `<name>,<factor>,<offset>`
* Parameters:
  * `<name>`: name of the `number`-typed register.
  * `<factor>`: floating point factor to apply to the register's value.
  * `<offset>`: floating point offset to apply to the register's value.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: factor parameter is not a valid double;
  * -6: offset parameter is not a valid double;
  * -7: register name not found, factor parameter set to 0, or register type different from "number";
  * -8: register name not found, or register type different from "number".

#### SetRegisterDecimals
* Description:
  * Set number of decimals for a `number`-typed register.
* Argument format:
  * `<name>,<decimals>`
* Parameters:
  * `<name>`: name of the `number`-typed register.
  * `<decimals>`: number of decimal digits to use when representing the register's value after applying factor and offset.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: decimals parameter is not a valid 8 bit unsigned integer;
  * -6: register name not found or register type different from "number".

#### SetMbConfig
* Description:
  * Set configuration for Modbus RTU.
* Argument format:
  * `<baudrate>[,<databits>,<parity>,<stopbits>,<bitOrder>]`
* Parameters:
  * `<baudrate>`: unsigned integer representing the baudrate.
  * `<databits>`: either 5,6,7 or 8, bits of data for each word.
  * `<parity>`: `even`, `odd` or `none` parity bit.
  * `<stopbits>`: either 1, 1.5 or 2 stop bits.
  * `<bitOrder>`: `msb` or `lsb`.
* Return values:
  * 1:  success;
  * -1: argument too long;
  * -2: too many parameters in argument;
  * -3: pointer to argument is NULL;
  * -4: wrong number of parameters;
  * -5: invalid baudrate;
  * -6: invalid data bits;
  * -7: invalid parity;
  * -8: invalid stop bits.
  * -9: invalid bitorder.

#### SetMbInterCmdsDelayMs
* Description:
  * Set pause between commands sent through Modbus RTU.
* Argument format:
  * `<delay>`
* Parameters:
  * `<delay>`: unsigned integer representing the pause in milliseconds.
* Return values:
  * 1:  success;
  * -1: delay parameter is not a valid 16 bit unsigned integer;
  * -2: delay parameter is not positive.

### GET
Methods available through GET calls.
#### GetRegistersList
* Description:
  * Get list of the names of the registers known to the gateway.
* Argument format:
  * none
* Parameters:
  * none
* Returns:
  * `["<name1>","<name2>",...,"<nameN>"]`: list of N names of the registers added to the gateway (may be empty is N=0).

#### GetRegisterDetails
* Description:
  * Get details of a register.
* Argument format:
  * `<name>`
* Parameters:
  * `<name>`: name of the register.
* Returns:
  * JSON object containing following keys:
    * `name`: name of the register;
    * `address`: Modbus RTU address of the slave owning the register;
    * `register`: Modbus RTU identifier of the register on the slave;
    * `readFunction`: Modbus RTU read function code;
    * `type`: type of the value held by the register;
    * `signed`: `true` if value in register is signed, `false` otherwise (only if type is `number`);
    * `factor`: multiplying factor of register value (only if type is `number`);
    * `offset`: additive offset of register value (only if type is `number`);
    * `decimals`: number of decimal digits in the register's representation (only if type is `number`);
    * `monitored`: `true` if value in register is monitored, `false` otherwise;
    * `maxPublishDelay`: maximum time that can pass, in seconds, before register's value is published again, 0 means disabled (only if monitored is `true`)
    * `publishOnChange`: `true` if value in register is monitored and must be published when it changes, `false` otherwise (only if monitored is `true`);
    * `changeCheckInterval`: seconds between a check of a Modbus register for changes and the next (only if publishOnChange is `true`);
    * `writable`: `true` if register can be written, `false` otherwise.
    * `writeFunction`: Modbus RTU write function code (only if it's `writable`);

#### ReadRegisterValue
* Description:
  * Read value of a register considering its type. Read value is not kept in memory.
* Argument format:
  * `<name>`
* Parameters:
  * `<name>`: name of the register.
* Returns:
  * `{"name":"<name>","value":<value>}`: `<name>` of the read register and its `<value>` according to its type. Empty object if name not found or read error occurred.

#### ReadRawRegisterValue
* Description:
  * Read value of a register as 16 bit unsigned integer. Read value is not kept in memory.
* Argument format:
  * `<readFunction>,<slaveAddr>,<registerId>`
* Parameters:
  * `<readFunction>`: Modbus RTU read function code:
    * 1: Coils (FC=01)
    * 2: Discrete Inputs (FC=02)
    * 3: Multiple Holding Registers (FC=03)
    * 4: Input Registers (FC=04)
  * `<slaveAddr>`: Modbus address of the slave;
  * `<registerId>`: Modbus ID of the register;
* Returns:
  * `{"address":"<slaveAddr>","register":<registerId>,"value":<value>}`: `<slaveAddr>` and `<registerId>` are the echo of the parameters used, `<value>` is the 16 bit unsigned int representation of the value inside the register. On error, an object containing only `"error"` key is returned, and its value is a string containig a description of the error occurred.

#### ReadAllRegistersValues
* Description:
  * Read values of all added registers. Read values are not kept in memory.
* Argument format:
  * none
* Parameters:
  * none
* Returns:
  * `{"<name1>":<value1>,"<name2>":<value2>,...,"<nameN>":<valueN>}`: `<nameX>` is the name of each read register and its value is `<valueX>`, for each integer X in [1,N], where N is the number of added registers. Empty JSON object if no registers added or read error occurred.

#### GetAllMonitoredRegistersLatestValues
* Description:
  * Get latest published values of all monitored registers. Note that such values are the ones that appeared as events and that calling `Read` methods doesn't update latest read values returned by this method.
* Argument format:
  * none
* Parameters:
  * none
* Returns:
  * `{"<name1>":<value1>,"<name2>":<value2>,...,"<nameM>":<valueM>}`: `<nameX>` is the name of each monitored register and `<valueX>` is its latest published value, for each integer X in [1,M], where M is the number of monitored registers. Empty JSON object if no registers monitored or read error occurred. `<valueX>` is `null` if no value was published for `<nameX>` yet.

#### GetRegisterNameByMbDetails
* Description:
  * Get register name by Modbus slave address and register identifier.
* Argument format:
  * `<function>,<slaveAddr>,<regId>`
* Parameters:
  * `<function>`: Modbus read function specified on register creation;
  * `<slaveAddr>`: unsigned integer of address of the Modbus slave;
  * `<regId>`: unsigned integer of register identifier.
* Returns:
  * `{"register":<regId>,"address":<slaveAddr>,"name":"<name>"}`: `<name>` of the searched register, (`<regId>` and `<slaveAddr>` same as parameters). Empty if register not found. On error, an object containing only `"error"` key is returned, and its value is a string containig a description of the error occurred.


#### GetActualModbusConfig
* Description:
  * Get details about the Modbus RTU configuration that was loaded at startup from flash and is currently used by the bus.
* Argument format:
  * none
* Parameters:
  * none
* Returns:
  * JSON object containing following keys:
    * `running`: `true` if Modbus RTU initialized and started correctly, `false` otherwise;
    * `interCmdsDelayMs`: delay between Modbus commands in milliseconds;
    * `baudrate`: baudrate used by ModbusRTU.
    * `readPeriod`: period between monitored registers readings;
    * `dataBits`: number of bits in every UART symbol;
    * `stopBits`: number of bits for stop in UART;
    * `parity`: kind of parity used by UART;
    * `bitPosition`: `msb`if Most Significant register comes first, `lsb`if Least Significant Register comes first. It makes sense only for multi-registers registers.

#### GetNextModbusConfig
* Description:
  * Get details about the Modbus RTU configuration that is being changed using `SetMb` POST methods. This configuration will become the active ("actual") one after saving it on flash and rebooting the ESP32.
* Argument format:
  * none
* Parameters:
  * none
* Returns:
* JSON object containing following keys:
    * `interCmdsDelayMs`: delay between Modbus commands in milliseconds;
    * `baudrate`: baudrate used by ModbusRTU.
    * `readPeriod`: period between monitored registers readings;
    * `dataBits`: number of bits in every UART symbol;
    * `stopBits`: number of bits for stop in UART;
    * `parity`: kind of parity used by UART.
