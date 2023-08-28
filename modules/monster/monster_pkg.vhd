--! @file monster_pkg.vhd
--! @brief Monster (all your top are belong to BEL) package
--! @author Wesley W. Terpstra <w.terpstra@gsi.de>
--!
--! Copyright (C) 2013 GSI Helmholtz Centre for Heavy Ion Research GmbH
--!
--! This combines all the common GSI components together
--!
--------------------------------------------------------------------------------
--! This library is free software; you can redistribute it and/or
--! modify it under the terms of the GNU Lesser General Public
--! License as published by the Free Software Foundation; either
--! version 3 of the License, or (at your option) any later version.
--!
--! This library is distributed in the hope that it will be useful,
--! but WITHOUT ANY WARRANTY; without even the implied warranty of
--! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
--! Lesser General Public License for more details.
--!
--! You should have received a copy of the GNU Lesser General Public
--! License along with this library. If not, see <http://www.gnu.org/licenses/>.
---------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use std.textio.all;
use IEEE.std_logic_textio.all;

library work;
use work.wishbone_pkg.all;

package monster_pkg is

  type io_channel is (IO_GPIO, IO_LVDS, IO_FIXED, IO_VIRTUAL);
  type io_direction is (IO_OUTPUT, IO_INPUT, IO_INOUTPUT);
  type io_logic_level is (IO_TTL, IO_LVTTL, IO_LVDS, IO_NIM, IO_CMOS);
  type io_special_purpose is (IO_NONE, IO_TTL_TO_NIM, IO_CLK_IN_EN, IO_MTCA4_TRIG_BPL_PDN, IO_MTCA4_FAILSAFE_EN, IO_LIBERA_TRIG_OE, IO_MTCA4_BPL_BUF_OE, IO_I2C_USB_C);

  type t_io_mapping_table is
    record                                               -- Byte(s) = Bit(s)
      info_name         : std_logic_vector(95 downto 0); --      12 = 96
      info_special      : std_logic_vector(5 downto 0);  --       x = 6 \
      info_special_out  : std_logic;                     --       x = 1  |
      info_special_in   : std_logic;                     --       x = 1 /
      info_index        : std_logic_vector(7 downto 0);  --       1 = 8
      info_direction    : std_logic_vector(1 downto 0);  --       x = 2 \
      info_channel      : std_logic_vector(2 downto 0);  --       x = 3  \
      info_oe           : std_logic;                     --       x = 1   |
      info_term         : std_logic;                     --       x = 1  /
      info_res_bit      : std_logic;                     --       x = 1 /
      info_logic_level  : std_logic_vector(3 downto 0);  --       x = 4 \
      info_reserved     : std_logic_vector(3 downto 0);  --       x = 4 /
    end record;                                          --      16 = 128 total each entry
  type t_io_mapping_table_array is array (natural range <>) of t_io_mapping_table;

  type t_io_mapping_table_arg is
    record
      info_name         : string (1 to 11);
      info_special      : io_special_purpose;
      info_special_out  : boolean;
      info_special_in   : boolean;
      info_index        : integer range 0 to 255;
      info_direction    : io_direction;
      info_channel      : io_channel;
      info_oe           : boolean;
      info_term         : boolean;
      info_logic_level  : io_logic_level;
    end record;
  type t_io_mapping_table_arg_array is array (natural range <>) of t_io_mapping_table_arg;

  function to_io_slv(str : string) return std_logic_vector;
  function f_gen_io_table(input : t_io_mapping_table_arg_array; ios_total : natural) return t_io_mapping_table_array;

  function f_sub1(x : natural) return natural;
  function f_pick(x : boolean; y : integer; z : integer) return natural;
  function f_string_list_repeat(s : string; times : natural) return string;
  function f_report_wishbone_address(value : t_wishbone_address; msg : string) return std_logic;

  component monster is
    generic(
      g_family               : string; -- "Arria II" or "Arria V"
      g_project              : string;
      g_flash_bits           : natural;
      g_psram_bits           : natural := 24;
      g_ram_size             : natural := 131072;
      g_gpio_inout           : natural := 0;
      g_gpio_in              : natural := 0;
      g_gpio_out             : natural := 0;
      g_tlu_fifo_size        : natural := 256;
      g_lvds_inout           : natural := 0;
      g_lvds_in              : natural := 0;
      g_lvds_out             : natural := 0;
      g_fixed                : natural := 0;
      g_lvds_invert          : boolean := false;
      g_en_tlu               : boolean := true;
      g_en_pcie              : boolean := false;
      g_en_vme               : boolean := false;
      g_en_usb               : boolean := false;
      g_en_scubus            : boolean := false;
      g_en_mil               : boolean := false;
      g_en_oled              : boolean := false;
      g_en_lcd               : boolean := false;
      g_en_cfi               : boolean := false;
      g_en_ddr3              : boolean := false;
      g_en_ssd1325           : boolean := false;
      g_en_nau8811           : boolean := false;
      g_en_user_ow           : boolean := false;
      g_en_psram             : boolean := false;
      g_en_beam_dump         : boolean := false;
      g_en_i2c_wrapper       : boolean := false;
      g_num_i2c_interfaces   : integer := 1;
      g_dual_port_wr         : boolean := false;
      g_io_table             : t_io_mapping_table_arg_array(natural range <>);
      g_en_pmc               : boolean := false;
      g_a10_use_sys_fpll     : boolean := false;
      g_a10_use_ref_fpll     : boolean := false;
      g_a10_en_phy_reconf    : boolean := false;
      g_en_butis             : boolean := true;
      g_lm32_cores           : natural := 1;
      g_lm32_MSIs            : natural := 1;
      g_lm32_ramsizes        : natural := 131072/4; -- in 32b words
      g_lm32_init_files      : string; -- multiple init files must be seperated by a semicolon ';'
      g_lm32_profiles        : string; -- multiple profiles must be seperated by a semicolon ';'
      g_lm32_are_ftm         : boolean := false;
      g_en_tempsens          : boolean := false;
      g_delay_diagnostics    : boolean := false;
      g_en_eca               : boolean := true;
      g_en_wd_tmr            : boolean := false;
      g_en_timer             : boolean := false;
      g_en_eca_tap           : boolean := false;
      g_en_asmi              : boolean := false

    );
    port(
      -- Required: core signals
      core_clk_20m_vcxo_i    : in    std_logic;
      core_clk_125m_sfpref_i : in    std_logic;
      core_clk_125m_pllref_i : in    std_logic;
      core_clk_125m_local_i  : in    std_logic;
      core_rstn_i            : in    std_logic := '1';
      -- Optional clock outputs
      core_clk_wr_ref_o      : out   std_logic;
      core_clk_butis_o       : out   std_logic;
      core_clk_butis_t0_o    : out   std_logic;
      core_rstn_wr_ref_o     : out   std_logic;
      core_rstn_butis_o      : out   std_logic;
      core_clk_sys_o         : out   std_logic;
      core_clk_200m_o        : out   std_logic;
      core_clk_20m_o         : out   std_logic;
      core_debug_o           : out   std_logic_vector(15 downto 0);
      core_clk_debug_i       : in    std_logic := '0';
      -- debug
      debug_wr_pps_led       : out std_logic;
      debug_wr_pps_pair_p    : out std_logic;
      debug_wr_ref_clk       : out std_logic;
      -- Required: white rabbit pins
      wr_onewire_io          : inout std_logic;
      wr_sfp_sda_io          : inout std_logic;
      wr_sfp_scl_io          : inout std_logic;
      wr_sfp_det_i           : in    std_logic;
      wr_sfp_tx_o            : out   std_logic;
      wr_sfp_rx_i            : in    std_logic;
      wr_dac_sclk_o          : out   std_logic;
      wr_dac_din_o           : out   std_logic;
      wr_ndac_cs_o           : out   std_logic_vector(2 downto 1);
      -- Optional dual port white rabbit pins
      wr_aux_onewire_io      : inout std_logic;
      wr_aux_sfp_sda_io      : inout std_logic;
      wr_aux_sfp_scl_io      : inout std_logic;
      wr_aux_sfp_det_i       : in    std_logic := '0';
      wr_aux_sfp_tx_o        : out   std_logic;
      wr_aux_sfp_rx_i        : in    std_logic := '0';
      -- Optional WR features
      wr_ext_clk_i           : in    std_logic := '0'; -- 10MHz
      wr_ext_pps_i           : in    std_logic := '0';
      wr_uart_o              : out   std_logic;
      wr_uart_i              : in    std_logic := '1';
      -- SFP
      sfp_tx_disable_o       : out   std_logic := '0';
      sfp_tx_fault_i         : in    std_logic;
      sfp_los_i              : in    std_logic;
      sfp_aux_tx_disable_o   : out   std_logic := '0';
      sfp_aux_tx_fault_i     : in    std_logic := '1';
      sfp_aux_los_i          : in    std_logic := '1';
      wbar_phy_dis_o         : out   std_logic := '0';
      wbar_phy_aux_dis_o     : out   std_logic := '0';
      phy_rx_ready_o         : out   std_logic;
      phy_tx_ready_o         : out   std_logic;
      phy_aux_rx_ready_o     : out   std_logic;
      phy_aux_tx_ready_o     : out   std_logic;
      phy_debug_o            : out   std_logic;
      phy_debug_i            : in    std_logic_vector(7 downto 0) := (others => '0');
      -- GPIO for the board (inouts start at 0, dedicated in/outs come after)
      gpio_i                 : in    std_logic_vector(f_sub1(g_gpio_inout+g_gpio_in)  downto 0) := (others => '1');
      gpio_o                 : out   std_logic_vector(f_sub1(g_gpio_inout+g_gpio_out) downto 0);
      gpio_oen_o             : out   std_logic_vector(f_sub1(g_gpio_inout+g_gpio_out) downto 0);
      gpio_term_o            : out   std_logic_vector(f_sub1(g_gpio_inout+g_gpio_in)  downto 0);
      gpio_spec_in_o         : out   std_logic_vector(f_sub1(g_gpio_inout+g_gpio_in)  downto 0);
      gpio_spec_out_o        : out   std_logic_vector(f_sub1(g_gpio_inout+g_gpio_out) downto 0);
      -- LVDS for the board (inouts start at 0, dedicated in/outs come after)
      lvds_p_i               : in    std_logic_vector(f_sub1(g_lvds_inout+g_lvds_in)  downto 0) := (others => '1');
      lvds_n_i               : in    std_logic_vector(f_sub1(g_lvds_inout+g_lvds_in)  downto 0) := (others => '1');
      lvds_i_led_o           : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_in)  downto 0);
      lvds_p_o               : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_out) downto 0);
      lvds_n_o               : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_out) downto 0);
      lvds_o_led_o           : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_out) downto 0);
      lvds_oen_o             : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_out) downto 0);
      lvds_term_o            : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_in)  downto 0);
      lvds_spec_in_o         : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_in)  downto 0);
      lvds_spec_out_o        : out   std_logic_vector(f_sub1(g_lvds_inout+g_lvds_out) downto 0);
      -- Optional status LEDs
      led_link_up_o          : out   std_logic;
      led_link_act_o         : out   std_logic;
      led_track_o            : out   std_logic;
      led_pps_o              : out   std_logic;
      led_aux_link_up_o      : out   std_logic;
      led_aux_link_act_o     : out   std_logic;
      led_aux_track_o        : out   std_logic;
      led_aux_pps_o          : out   std_logic;
      -- g_en_pcie
      pcie_refclk_i          : in    std_logic := '0';
      pcie_rstn_i            : in    std_logic := '0';
      pcie_rx_i              : in    std_logic_vector(3 downto 0) := (others => '0');
      pcie_tx_o              : out   std_logic_Vector(3 downto 0);
      -- g_en_vme
      vme_as_n_i             : in    std_logic := '0';
      vme_rst_n_i            : in    std_logic := '0';
      vme_write_n_i          : in    std_logic := '1';
      vme_am_i               : in    std_logic_vector(5 downto 0) := (others => '0');
      vme_ds_n_i             : in    std_logic_vector(1 downto 0) := (others => '1');
      vme_ga_i               : in    std_logic_vector(3 downto 0) := (others => '0');
      vme_addr_data_b        : inout std_logic_vector(31 downto 0) := (others => 'Z');
      vme_iack_n_i           : in    std_logic := '1';
      vme_iackin_n_i         : in    std_logic := '1';
      vme_iackout_n_o        : out   std_logic;
      vme_irq_n_o            : out   std_logic_vector(6 downto 0);
      vme_berr_o             : out   std_logic;
      vme_dtack_oe_o         : out   std_logic;
      vme_buffer_latch_o     : out   std_logic_vector(3 downto 0);
      vme_data_oe_ab_o       : out   std_logic;
      vme_data_oe_ba_o       : out   std_logic;
      vme_addr_oe_ab_o       : out   std_logic;
      vme_addr_oe_ba_o       : out   std_logic;
      -- g_en_usb
      usb_rstn_o             : out   std_logic;
      usb_ebcyc_i            : in    std_logic := '0';
      usb_speed_i            : in    std_logic := '0';
      usb_shift_i            : in    std_logic := '0';
      usb_readyn_io          : inout std_logic := 'Z';
      usb_fifoadr_o          : out   std_logic_vector(1 downto 0);
      usb_sloen_o            : out   std_logic;
      usb_fulln_i            : in    std_logic := '1';
      usb_emptyn_i           : in    std_logic := '0';
      usb_slrdn_o            : out   std_logic;
      usb_slwrn_o            : out   std_logic;
      usb_pktendn_o          : out   std_logic;
      usb_fd_io              : inout std_logic_vector(7 downto 0) := (others => 'Z');
      -- g_en_scubus
      scubus_a_a             : out   std_logic_vector(15 downto 0);
      scubus_a_d             : inout std_logic_vector(15 downto 0) := (others => 'Z');
      scubus_nsel_data_drv   : out   std_logic;
      scubus_a_nds           : out   std_logic;
      scubus_a_rnw           : out   std_logic;
      scubus_a_ndtack        : in    std_logic := '1';
      scubus_a_nsrq          : in    std_logic_vector(12 downto 1) := (others => '1');
      scubus_a_nsel          : out   std_logic_vector(12 downto 1);
      scubus_a_ntiming_cycle : out   std_logic;
      scubus_a_sysclock      : out   std_logic;
      -- g_en_mil
      mil_nme_boo_i          : in    std_logic := '0';
      mil_nme_bzo_i          : in    std_logic := '0';
      mil_me_sd_i            : in    std_logic := '0';
      mil_me_esc_i           : in    std_logic := '0';
      mil_me_sdi_o           : out   std_logic;
      mil_me_ee_o            : out   std_logic;
      mil_me_ss_o            : out   std_logic;
      mil_me_boi_o           : out   std_logic;
      mil_me_bzi_o           : out   std_logic;
      mil_me_udi_o           : out   std_logic;
      mil_me_cds_i           : in    std_logic := '0';
      mil_me_sdo_i           : in    std_logic := '0';
      mil_me_dsc_i           : in    std_logic := '0';
      mil_me_vw_i            : in    std_logic := '0';
      mil_me_td_i            : in    std_logic := '0';
      mil_me_12mhz_o         : out   std_logic;
      mil_boi_i              : in    std_logic := '0';
      mil_bzi_i              : in    std_logic := '0';
      mil_sel_drv_o          : out   std_logic;
      mil_nsel_rcv_o         : out   std_logic;
      mil_nboo_o             : out   std_logic;
      mil_nbzo_o             : out   std_logic;
      mil_nled_rcv_o         : out   std_logic;
      mil_nled_trm_o         : out   std_logic;
      mil_nled_err_o         : out   std_logic;
      mil_timing_i           : in    std_logic := '0';
      mil_nled_timing_o      : out   std_logic;
      mil_nled_fifo_ne_o     : out   std_logic;
      mil_interlock_intr_i   : in    std_logic := '0';
      mil_data_rdy_intr_i    : in    std_logic := '0';
      mil_data_req_intr_i    : in    std_logic := '0';
      mil_nled_interl_o      : out   std_logic;
      mil_nled_dry_o         : out   std_logic;
      mil_nled_drq_o         : out   std_logic;
      mil_lemo_data_o        : out   std_logic_vector(4 downto 1);
      mil_lemo_nled_o        : out   std_logic_vector(4 downto 1);
      mil_lemo_out_en_o      : out   std_logic_vector(4 downto 1);
      mil_lemo_data_i        : in    std_logic_vector(4 downto 1):= (others => '0');
--      mil_io1_o              : out   std_logic;
--      mil_io1_is_in_o        : out   std_logic;
--      mil_nled_io1_o         : out   std_logic;
--      mil_io2_o              : out   std_logic;
--      mil_io2_is_in_o        : out   std_logic;
--      mil_nled_io2_o         : out   std_logic;
      -- g_en_oled
      oled_rstn_o            : out   std_logic;
      oled_dc_o              : out   std_logic;
      oled_ss_o              : out   std_logic;
      oled_sck_o             : out   std_logic;
      oled_sd_o              : out   std_logic;
      oled_sh_vr_o           : out   std_logic;
      -- g_en_lcd
      lcd_scp_o              : out   std_logic;
      lcd_lp_o               : out   std_logic;
      lcd_flm_o              : out   std_logic;
      lcd_in_o               : out   std_logic;
      -- g_en_ssd1325
      ssd1325_rst_o          : out   std_logic;
      ssd1325_dc_o           : out   std_logic;
      ssd1325_ss_o           : out   std_logic;
      ssd1325_sclk_o         : out   std_logic;
      ssd1325_data_o         : out   std_logic;
      -- g_en_nau8811
      nau8811_spi_csb_o      : out   std_logic;
      nau8811_spi_sclk_o     : out   std_logic;
      nau8811_spi_sdio_o     : out   std_logic;
      nau8811_iis_fs_o       : out   std_logic;
      nau8811_iis_bclk_o     : out   std_logic;
      nau8811_iis_adcout_o   : out   std_logic;
      nau8811_iis_dacin_i    : in    std_logic := '0';
      -- g_en_cfi
      cfi_ad                 : out   std_logic_vector(25 downto 1);
      cfi_df                 : inout std_logic_vector(15 downto 0) := (others => 'Z');
      cfi_adv_fsh            : out   std_logic ;
      cfi_nce_fsh            : out   std_logic ;
      cfi_clk_fsh            : out   std_logic ;
      cfi_nwe_fsh            : out   std_logic ;
      cfi_noe_fsh            : out   std_logic ;
      cfi_nrst_fsh           : out   std_logic ;
      cfi_wait_fsh           : in    std_logic := '0';
      -- g_en_ddr3
      mem_DDR3_DQ            : inout std_logic_vector(15 downto 0);
      mem_DDR3_DM            : out   std_logic_vector( 1 downto 0);
      mem_DDR3_BA            : out   std_logic_vector( 2 downto 0);
      mem_DDR3_ADDR          : out   std_logic_vector(12 downto 0);
      mem_DDR3_CS_n          : out   std_logic_vector( 0 downto 0);
      mem_DDR3_DQS           : inout std_logic_vector( 1 downto 0);
      mem_DDR3_DQSn          : inout std_logic_vector( 1 downto 0);
      mem_DDR3_RES_n         : out   std_logic;
      mem_DDR3_CKE           : out   std_logic_vector( 0 downto 0);
      mem_DDR3_ODT           : out   std_logic_vector( 0 downto 0);
      mem_DDR3_CAS_n         : out   std_logic;
      mem_DDR3_RAS_n         : out   std_logic;
      mem_DDR3_CLK           : inout std_logic_vector( 0 downto 0);
      mem_DDR3_CLK_n         : inout std_logic_vector( 0 downto 0);
      mem_DDR3_WE_n          : out   std_logic;
      -- g_en_psram
      ps_clk                 : out   std_logic;
      ps_addr                : out   std_logic_vector(g_psram_bits-1 downto 0);
      ps_data                : inout std_logic_vector(15 downto 0) := (others => 'Z');
      ps_seln                : out   std_logic_vector(1 downto 0);
      ps_cen                 : out   std_logic;
      ps_oen                 : out   std_logic;
      ps_wen                 : out   std_logic;
      ps_cre                 : out   std_logic;
      ps_advn                : out   std_logic;
      ps_wait                : in    std_logic := '0';
      -- i2c
      i2c_scl_pad_i          : in    std_logic_vector(g_num_i2c_interfaces-1 downto 0) := (others => '0');
      i2c_scl_pad_o          : out   std_logic_vector(g_num_i2c_interfaces-1 downto 0);
      i2c_scl_padoen_o       : out   std_logic_vector(g_num_i2c_interfaces-1 downto 0);
      i2c_sda_pad_i          : in    std_logic_vector(g_num_i2c_interfaces-1 downto 0) := (others => '0');
      i2c_sda_pad_o          : out   std_logic_vector(g_num_i2c_interfaces-1 downto 0);
      i2c_sda_padoen_o       : out   std_logic_vector(g_num_i2c_interfaces-1 downto 0);
      -- g_en_pmc
      pmc_pci_clk_i          : in    std_logic := '0';
      pmc_pci_rst_i          : in    std_logic := '0';
      pmc_buf_oe_o           : out   std_logic;
      pmc_busmode_io         : inout std_logic_vector(3 downto 0) := (others => 'Z');
      pmc_ad_io              : inout std_logic_vector(31 downto 0) := (others => 'Z');
      pmc_c_be_io            : inout std_logic_vector(3 downto 0) := (others => 'Z');
      pmc_par_io             : inout std_logic := 'Z';
      pmc_frame_io           : inout std_logic := 'Z';
      pmc_trdy_io            : inout std_logic := 'Z';
      pmc_irdy_io            : inout std_logic := 'Z';
      pmc_stop_io            : inout std_logic := 'Z';
      pmc_devsel_io          : inout std_logic := 'Z';
      pmc_idsel_i            : in    std_logic := '0';
      pmc_perr_io            : inout std_logic := 'Z';
      pmc_serr_io            : inout std_logic := 'Z';
      pmc_inta_o             : out   std_logic;
      pmc_req_o              : out   std_logic;
      pmc_gnt_i              : in    std_logic := '1';
      -- g_en_user_ow
      ow_io                  : inout std_logic_vector(1 downto 0) := (others => 'Z');
      hw_version             : in std_logic_vector(31 downto 0) := (others => 'Z'));
  end component;

  constant c_user_1wire_sdb : t_sdb_device := (
    abi_class     => x"0000",              -- undocumented device
    abi_ver_major => x"01",
    abi_ver_minor => x"01",
    wbd_endian    => c_sdb_endian_big,
    wbd_width     => x"7",                 -- 8/16/32-bit port granularity
    sdb_component => (
      addr_first  => x"0000000000000000",
      addr_last   => x"00000000000000ff",
      product     => (
        vendor_id => x"0000000000000651",  -- GSI
        device_id => x"4c8a0635",
        version   => x"00000001",
        date      => x"20171016",
        name      => "User-1Wire         ")));

  constant c_iodir_sdb : t_sdb_device := (
    abi_class     => x"0000", -- undocumented device
    abi_ver_major => x"00",
    abi_ver_minor => x"00",
    wbd_endian    => c_sdb_endian_big,
    wbd_width     => x"7", -- 8/16/32-bit port granularity
    sdb_component => (
    addr_first    => x"0000000000000000",
    addr_last     => x"000000000000000f",
    product => (
    vendor_id     => x"0000000000000651",
    device_id     => x"4d78adfd",
    version       => x"00000001",
    date          => x"20140516",
    name          => "GSI:IODIR_HACK     ")));

  component monster_iodir is
    generic(
      g_gpio_inout : natural := 0;
      g_lvds_inout : natural := 0);
    port(
      clk_i      : in  std_logic;
      rst_n_i    : in  std_logic;
      slave_i    : in  t_wishbone_slave_in;
      slave_o    : out t_wishbone_slave_out;
      gpio_oen_o : out std_logic_vector(f_sub1(g_gpio_inout) downto 0);
      lvds_oen_o : out std_logic_vector(f_sub1(g_lvds_inout) downto 0));
  end component;

end package;

package body monster_pkg is

  function f_sub1(x : natural) return natural is
  begin
    if x = 0
    then return 0;
    else return x-1;
    end if;
  end f_sub1;

  function f_pick(x : boolean; y : integer; z : integer) return natural is
  begin
    if x
    then return y;
    else return z;
    end if;
  end f_pick;

  function to_io_slv(str : string) return std_logic_vector is
    alias str_norm   : string(1 to str'length) is str;
    variable res_v   : std_logic_vector(8 * (str'length+1) - 1 downto 0);
    variable res_v_r : std_logic_vector(8 * (str'length+1) - 1 downto 0);
  begin
    for idx in 1 to (str'length+1) loop
      if idx = (str'length+1) then
        res_v(8 * idx - 1 downto 8 * idx - 8) := (others => '0'); -- Terminate string with zero
      else
        res_v(8 * idx - 1 downto 8 * idx - 8) := std_logic_vector(to_unsigned(character'pos(str_norm(idx)), 8));
        if std_logic_vector(to_unsigned(character'pos(str_norm(idx)), 8)) = x"20" then -- Check for space
          res_v(8 * idx - 1 downto 8 * idx - 8) := (others => '0'); -- Fill string with zeros
        end if;
      end if;
    end loop;
    -- Reverse byte order if needed
    for idx in 1 to (str'length+1) loop
      res_v_r(95+8 -(8 * idx) downto 95+1 - (8 * idx)) := res_v(8 * idx - 1 downto 8 * idx - 8);
    end loop;
    return res_v_r;
  end function;

  function f_gen_io_table(input : t_io_mapping_table_arg_array; ios_total : natural) return t_io_mapping_table_array
  is
    variable result      : t_io_mapping_table_array(0 to ios_total);
    variable name        : string (1 to 11);
    variable special     : integer range 0 to 63;
    variable direction   : integer range 0 to 3;
    variable channel     : integer range 0 to 7;
    variable logic_level : integer range 0 to 15;
  begin
    for i in 0 to ios_total-1 loop
      report "IO ITERATOR: " & integer'image(i) severity note;
      report "IO NAME: " & name severity note;
      -- Convert name
      name := input(i).info_name;
      result(i).info_name:= to_io_slv(name);
      -- Convert special information
      case input(i).info_special is
        when IO_NONE               => special := 0;
        when IO_TTL_TO_NIM         => special := 1;
        when IO_CLK_IN_EN          => special := 2;
        when IO_MTCA4_TRIG_BPL_PDN => special := 3;
        when IO_MTCA4_FAILSAFE_EN  => special := 4;
        when IO_LIBERA_TRIG_OE     => special := 5;
        when IO_MTCA4_BPL_BUF_OE   => special := 6;
        when IO_I2C_USB_C          => special := 7;
        when others                => special := 63;
      end case;
      result(i).info_special := std_logic_vector(to_unsigned(special, result(i).info_special'length));
      if input(i).info_special_out = true then
        result(i).info_special_out := '1';
      else
        result(i).info_special_out := '0';
      end if;
      if input(i).info_special_in = true then
        result(i).info_special_in := '1';
      else
        result(i).info_special_in := '0';
      end if;
      -- Convert Index
      result(i).info_index := std_logic_vector(to_unsigned(input(i).info_index, result(i).info_index'length));
      -- Convert Direction
      case input(i).info_direction is
        when IO_OUTPUT   => direction := 0;
        when IO_INPUT    => direction := 1;
        when IO_INOUTPUT => direction := 2;
        when others      => direction := 3;
      end case;
      result(i).info_direction := std_logic_vector(to_unsigned(direction, result(i).info_direction'length));
      -- Convert Channel
      case input(i).info_channel is
        when IO_GPIO    => channel := 0;
        when IO_LVDS    => channel := 1;
        when IO_FIXED   => channel := 2;
        when IO_VIRTUAL => channel := 3;
        when others     => channel := 7;
      end case;
      result(i).info_channel := std_logic_vector(to_unsigned(channel, result(i).info_channel'length));
      -- Convert OutputEnable
      if input(i).info_oe = true then
        result(i).info_oe := '1';
      else
        result(i).info_oe := '0';
      end if;
      -- Convert Termination
      if input(i).info_term = true then
        result(i).info_term := '1';
      else
        result(i).info_term := '0';
      end if;
      -- Convert Reserved Bit
      result(i).info_res_bit := '0';
      -- Convert Logic Level
      case input(i).info_logic_level is
        when IO_TTL   => logic_level := 0;
        when IO_LVTTL => logic_level := 1;
        when IO_LVDS  => logic_level := 2;
        when IO_NIM   => logic_level := 3;
        when IO_CMOS  => logic_level := 4;
        when others   => logic_level := 15;
      end case;
      result(i).info_logic_level := std_logic_vector(to_unsigned(logic_level, result(i).info_logic_level'length));
      -- Convert Reserved Vector
      result(i).info_reserved := "0000";
    end loop;
    --report "DONE " & name severity failure;
    return result;
  end f_gen_io_table;

  function f_string_list_repeat(s : string; times : natural)
  return string is
    variable i : natural := 0;
	 constant delimeter : string := ";";
	 constant str : string := s & delimeter;
    variable res : string(1 to str'length*times);
  begin
    for i in 0 to times-1 loop
      res(i*str'length+1 to (i+1)*str'length) := str;
    end loop;
    return res;
  end f_string_list_repeat;

  function f_report_wishbone_address(value : t_wishbone_address; msg : string)
  return std_logic is
  begin
    report "Debug: " & msg;
    report "Debug: Wishbone address (dec) = " & integer'image(to_integer(unsigned(value)));
    return '0';
  end f_report_wishbone_address;

end monster_pkg;
