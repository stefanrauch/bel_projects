library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;
--use work.monster_pkg.all;

package wb_dma_pkg is

  constant c_wb_dma_sdb : t_sdb_device := (
    abi_class     => x"0000", -- undocumented device
    abi_ver_major => x"01",
    abi_ver_minor => x"00",
    wbd_endian    => c_sdb_endian_big,
    wbd_width     => x"7", -- 8/16/32-bit port granularity
    sdb_component => (
    addr_first    => x"0000000000000000",
    addr_last     => x"00000000000000ff",
    product       => (
    vendor_id     => x"0000000000000651",
    device_id     => x"434E5450", -- !TODO change
    version       => x"00000001",
    date          => x"20240904",
    name          => "GSI:WB_DMA         "))
    );
    
  component wb_dma is
    port(
      clk_i       : in std_logic;
      rst_i       : in std_logic;
      
      wb0s_data_i : in t_wishbone_data;
      wb0s_data_o : out t_wishbone_data;
      wb0_addr_i  : in t_wishbone_address;
      wb0_sel_i   : in t_wishbone_byte_select;
      wb0_we_i    : in std_logic;
      wb0_cyc_i   : in std_logic;
	    wb0_stb_i   : in std_logic;
      wb0_ack_o   : out std_logic;
      wb0_err_o   : out std_logic;
      wb0_rty_o   : out std_logic;
	    wb0m_data_i : in t_wishbone_data;
      wb0m_data_o : out t_wishbone_data;
      wb0_addr_o  : out t_wishbone_address;
      wb0_sel_o   : out t_wishbone_byte_select;
      wb0_we_o    : out std_logic;
      wb0_cyc_o   : out std_logic;
	    wb0_stb_o   : out std_logic;
      wb0_ack_i   : in std_logic;
      wb0_err_i   : in std_logic;
      wb0_rty_i   : in std_logic;

	    wb1s_data_i : in t_wishbone_data;
      wb1s_data_o : out t_wishbone_data;
      wb1_addr_i  : in t_wishbone_address;
      wb1_sel_i   : in t_wishbone_byte_select;
      wb1_we_i    : in std_logic;
      wb1_cyc_i   : in std_logic;
	    wb1_stb_i   : in std_logic;
      wb1_ack_o   : out std_logic;
      wb1_err_o   : out std_logic;
      wb1_rty_o   : out std_logic;
	    wb1m_data_i : in t_wishbone_data;
      wb1m_data_o : out t_wishbone_data;
      wb1_addr_o  : out t_wishbone_address;
      wb1_sel_o   : out t_wishbone_byte_select;
      wb1_we_o    : out std_logic;
      wb1_cyc_o   : out std_logic;
	    wb1_stb_o   : out std_logic;
      wb1_ack_i   : in std_logic;
      wb1_err_i   : in std_logic;
      wb1_rty_i   : in std_logic;

      dma_req_i   : in std_logic;
      dma_ack_o   : out std_logic;
      dma_nd_i    : in std_logic;
      dma_rest_i  : in std_logic;

	    inta_o      : out std_logic;
      intb_o      : out std_logic
      );
  end component;

end package;
