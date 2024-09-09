library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;
--use work.monster_pkg.all;

package virtualRAM_pkg is

  constant c_virtualRAM_sdb : t_sdb_device := (
    abi_class     => x"0000", -- undocumented device
    abi_ver_major => x"01",
    abi_ver_minor => x"00",
    wbd_endian    => c_sdb_endian_big,
    wbd_width     => x"7", -- 8/16/32-bit port granularity
    sdb_component => (
    addr_first    => x"0000000000000000",
    addr_last     => x"00000000000007ff",
    product       => (
    vendor_id     => x"0000000000000651",
    device_id     => x"434E5449", -- !TODO change
    version       => x"00000001",
    date          => x"20240904",
    name          => "GSI:VIRTUALRAM     "))
    );
    
    
  component virtualRAM is
    generic (
      g_size  : natural := 2048
    );

    port(
      clk_sys_i : in std_logic;
      rst_n_i   : in std_logic;
  
      slave_i : in  t_wishbone_slave_in;
      slave_o : out t_wishbone_slave_out
      );
  end component;

end package;
