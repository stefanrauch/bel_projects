library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.monster_pkg.all;
use work.altera_lvds_pkg.all;
use work.ramsize_pkg.c_lm32_ramsizes;
use work.altera_networks_pkg.all;

entity pexp_control is
  generic(
    g_LOAD_SHIFT_REG_EN : boolean := false
  );
  port(
    -----------------------------------------
    -- Clocks
    -----------------------------------------
    clk_20m_vcxo_i    : in std_logic; -- 20MHz VCXO clock (BANK 3A - AF21)
    clk_125m_pllref_i : in std_logic; -- 125 MHz PLL reference - (clk_125m_wrpll_0  on sch)
    clk_125m_local_i  : in std_logic; -- local clk from 125Mhz oszillator (clk_osc_1  on sch)
    clk_sfp_ref_i     : in std_logic; -- SFP clk (clk_125m_wrpll_1 on sch)
    clk_lvtio_i       : in std_logic; -- LEMO front panel input

--    clk_osc_1_i         : in std_logic;  -- local clk from 100MHz or 125Mhz oscillator

    -----------------------------------------------------------------------
    -- reset
    -----------------------------------------------------------------------
    fpga_res_i        : in std_logic;
    nres_i            : in std_logic;

    -----------------------------------------------------------------------
    -- SFP
    -----------------------------------------------------------------------

    sfp_tx_dis_o     : out std_logic;
    sfp_tx_fault_i   : in  std_logic;
    sfp_los_i        : in  std_logic;

    sfp_txp_o        : out std_logic;
    sfp_rxp_i        : in  std_logic;

    sfp_mod0_i       : in    std_logic;  -- grounded by module
    sfp_mod1_io      : inout std_logic;  -- SCL
    sfp_mod2_io      : inout std_logic;  -- SDA

    ------------------------------------------------------------------------
    -- WR DAC signals
    ------------------------------------------------------------------------
    wr_dac_sclk_o  : out std_logic;
    wr_dac_din_o   : out std_logic;
    wr_ndac_cs_o   : out std_logic_vector(2 downto 1);

    -----------------------------------------------------------------------
    -- OneWire
    -----------------------------------------------------------------------
    rom_data_io        : inout std_logic;

    -----------------------------------------------------------------------
    -- lcd display
    -----------------------------------------------------------------------
    dis_di_o        : out std_logic_vector(6 downto 0);
    dis_ai_i        : in  std_logic_vector(1 downto 0);
    dis_do_i        : in  std_logic;
    dis_wr_o        : out std_logic := '0';
    dis_rst_o       : out std_logic := '1';

    -----------------------------------------------------------------------
    -- connector cpld
    -----------------------------------------------------------------------
    con             : in std_logic_vector(5 downto 1);

    -----------------------------------------------------------------------
    -- logic analyzer
    -----------------------------------------------------------------------
    hpwck           : out   std_logic := 'Z';
    hpw             : inout std_logic_vector(15 downto 0) := (others => 'Z'); -- logic analyzer

    -----------------------------------------------------------------------
    -- hex switch
    -----------------------------------------------------------------------
    hswf_i          : in std_logic_vector(4 downto 1);

    -----------------------------------------------------------------------
    -- push buttons
    -----------------------------------------------------------------------
    pbs_f_i         : in std_logic;

    -----------------------------------------------------------------------
    -- usb
    -----------------------------------------------------------------------
    slrd              : out   std_logic;
    slwr              : out   std_logic;
    fd                : inout std_logic_vector(7 downto 0) := (others => 'Z');
    pa                : inout std_logic_vector(7 downto 0) := (others => 'Z');
    ctl               : in    std_logic_vector(2 downto 0);
    uclk              : in    std_logic;
    ures              : out   std_logic;
    ifclk             : inout std_logic := 'Z';
    wakeup            : inout std_logic := 'Z';

    -----------------------------------------------------------------------
    -- leds on board - user
    -----------------------------------------------------------------------
    led_user_o      : out std_logic_vector(8 downto 1) := (others => '0');

    -----------------------------------------------------------------------
    -- leds on front panel - status
    -----------------------------------------------------------------------
    led_status_o      : out std_logic_vector(6 downto 1);

    -----------------------------------------------------------------------
    -- lvds/lvttl lemos on front panel
    -----------------------------------------------------------------------
    lvtio_in_n_i     : in  std_logic_vector(5 downto 1);
    lvtio_in_p_i     : in  std_logic_vector(5 downto 1);
    lvtio_out_n_o    : out std_logic_vector(5 downto 1);
    lvtio_out_p_o    : out std_logic_vector(5 downto 1);
    lvtio_oe_n_o     : out std_logic_vector(5 downto 1);
    lvtio_term_en_o  : out std_logic_vector(5 downto 1);
    lvtio_led_act_o  : out std_logic_vector(5 downto 1);
    lvtio_led_dir_o  : out std_logic_vector(5 downto 1);

    -- enable clock input from front panel LEMO
    lvt_in_clk_en_n_o   : out std_logic;



    -----------------------------------------------------------------------
    -----------------------------------------------------------------------
    -- form factor specific pins/interfaces
    -----------------------------------------------------------------------
    -----------------------------------------------------------------------

    -----------------------------------------
    -- host bus interface PCIe
    -----------------------------------------

    pcie_refclk_i  : in  std_logic;
    pcie_rx_i      : in  std_logic_vector(3 downto 0);
    pcie_tx_o      : out std_logic_vector(3 downto 0);
    nPCI_RESET     : in std_logic;

    pe_smdat        : inout std_logic;
    pe_snclk        : out std_logic;
    pe_waken        : out std_logic

    );
end pexp_control;

architecture rtl of pexp_control is

  constant c_HWT_EN_BIT : natural := 8;

  signal clk_sys       : std_logic;
  signal clk_200m      : std_logic;
  signal clk_10m       : std_logic;

  signal s_led_status_monster : std_logic_vector(6 downto 1);
  signal s_led_user_monster   : std_logic_vector(8 downto 1);

  signal s_led_status         : std_logic_vector(6 downto 1);
  signal s_led_user           : std_logic_vector(8 downto 1);

  signal s_gpio_out           : std_logic_vector(8 downto 0);
  signal s_gpio_in            : std_logic_vector(9 downto 0);

  signal s_test_sel     : std_logic_vector(4 downto 0);

  -- white rabbit status leds
  signal s_led_link_up  : std_logic;
  signal s_led_link_act : std_logic;
  signal s_led_track    : std_logic;
  signal s_led_pps      : std_logic;

  -- front panel io leds
  signal s_led_frnt_red  : std_logic;
  signal s_led_frnt_blue : std_logic;

  -- user leds (on board)
  signal s_leds_user    : std_logic_vector(3 downto 0);

  -- io differential and control signals
  signal s_lvds_p_i     : std_logic_vector(4 downto 0);
  signal s_lvds_n_i     : std_logic_vector(4 downto 0);
  signal s_lvds_i_led   : std_logic_vector(4 downto 0);
  signal s_lvds_term_en : std_logic_vector(4 downto 0);

  signal s_lvds_p_o     : std_logic_vector(4 downto 0);
  signal s_lvds_n_o     : std_logic_vector(4 downto 0);
  signal s_lvds_o_led   : std_logic_vector(4 downto 0);
  signal s_lvds_oe      : std_logic_vector(4 downto 0);

  signal s_lvds_led     : std_logic_vector(4 downto 0);

  signal s_wr_ext_in    : std_logic;

  -- logic analyzer
  signal s_log_oe   : std_logic_vector(16 downto 0);
  signal s_log_out  : std_logic_vector(16 downto 0);
  signal s_log_in   : std_logic_vector(16 downto 0);



  constant io_mapping_table : t_io_mapping_table_arg_array(0 to 25) :=
  (
  -- Name[11 Bytes], Special Purpose, SpecOut, SpecIn, Index, Direction,   Channel,  OutputEnable, Termination, Logic Level
    ("LED1       ", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ), -- user LEDs
    ("LED2       ", IO_NONE,         false,   false,  1,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ),
    ("LED3       ", IO_NONE,         false,   false,  2,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ),
    ("LED4       ", IO_NONE,         false,   false,  3,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ),
    ("LED5       ", IO_NONE,         false,   false,  4,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ),
    ("LED6       ", IO_NONE,         false,   false,  5,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ),
    ("LED7       ", IO_NONE,         false,   false,  6,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ),
    ("LED8       ", IO_NONE,         false,   false,  7,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ),
    ("HWT_EN     ", IO_NONE,         false,   false,  8,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL  ), -- enable HW test
    ("HSF0       ", IO_NONE,         false,   false,  0,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL), -- FPGA HEX switch
    ("HSF1       ", IO_NONE,         false,   false,  1,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL),
    ("HSF2       ", IO_NONE,         false,   false,  2,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL),
    ("HSF3       ", IO_NONE,         false,   false,  3,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL),
    ("HSP0       ", IO_NONE,         false,   false,  4,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL), -- CPLD HEX switch
    ("HSP1       ", IO_NONE,         false,   false,  5,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL),
    ("HSP2       ", IO_NONE,         false,   false,  6,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL),
    ("HSP3       ", IO_NONE,         false,   false,  7,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL),
    ("PBF        ", IO_NONE,         false,   false,  8,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL), -- FPGA push button
    ("PBC        ", IO_NONE,         false,   false,  9,     IO_INPUT,    IO_GPIO,  false,        false,       IO_LVTTL), -- CPLD push button
    ("IO1        ", IO_NONE,         false,   false,  0,     IO_INOUTPUT, IO_LVDS,  true,         true,        IO_LVTTL), -- front panel IOs
    ("IO2        ", IO_NONE,         false,   false,  1,     IO_INOUTPUT, IO_LVDS,  true,         true,        IO_LVTTL),
    ("IO3        ", IO_NONE,         false,   false,  2,     IO_INOUTPUT, IO_LVDS,  true,         true,        IO_LVTTL),
    ("IO4        ", IO_NONE,         false,   false,  3,     IO_INPUT,    IO_LVDS,  false,        false,       IO_LVTTL),
    ("IO5        ", IO_NONE,         false,   false,  4,     IO_INPUT,    IO_LVDS,  false,        false,       IO_LVTTL),
    ("IO4_PPS    ", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_FIXED, false,        false,       IO_LVTTL),
    ("IO5_10MHZ  ", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_FIXED, false,        false,       IO_LVTTL)
  );


  constant c_family     : string := "Arria V";
  constant c_project    : string := "pexp_control";
  constant c_cores      : natural:= 1;
  constant c_initf_name : string := c_project & "_stub.mif";
  constant c_profile_name : string := "medium_icache_debug";

  -- projectname is standard to ensure a stub mif that prevents unwanted scanning of the bus
  -- multiple init files for n processors are to be seperated by semicolon ';'


  constant c_LOAD_SHIFT_REG_WIDTH : positive := 16;
  constant c_LOAD_SHIFT_REG_DEPTH : positive := 9*(2**9);

  type t_bit_array is array (natural range<>) of std_logic_vector(c_LOAD_SHIFT_REG_DEPTH-1 downto 0);

  signal s_dis_led_green : std_logic;
  signal s_dis_led_red   : std_logic;
  signal s_dis_led_blue  : std_logic;

  signal s_blink_counter       : unsigned(27 downto 0);
  signal s_reg_blink_counter   : unsigned(27 downto 0);

  signal s_load_shift_en      : std_logic;
  signal s_load_shift_input   : std_logic_vector(c_LOAD_SHIFT_REG_WIDTH-1 downto 0);
  signal s_shift_reg_out      : std_logic_vector(c_LOAD_SHIFT_REG_WIDTH-1 downto 0);
  signal s_load_shift_reg_arr : t_bit_array(c_LOAD_SHIFT_REG_WIDTH-1 downto 0);
  signal s_pseudo_rand_reg 	  : std_logic_vector(31 downto 0) := (others => '0');
  signal s_led_reg_en         : std_logic;
  signal s_shift_reg_to_leds  : std_logic_vector(15 downto 0);

  signal core_debug_out       : std_logic_vector(15 downto 0);



begin

  main : monster
    generic map(
      g_family            => c_family,
      g_project           => c_project,
      g_flash_bits        => 25,
      g_lvds_inout        => 3,  -- 5 LEMOs at front panel
      g_lvds_in           => 2,
      g_lvds_out          => 0,
      g_gpio_out          => 9,  -- 8 on-boards LEDs, internal HW test enable
      g_gpio_in           => 10, -- FPGA button and HEX switch (1+4), CPLD button and HEX switch (1+4)
      g_fixed             => 2,
      g_lvds_invert       => false,
      g_en_usb            => true,
      g_en_lcd            => true,
      g_en_user_ow        => false,
      g_en_tempsens       => true,
      g_en_pcie           => true,
      g_delay_diagnostics => true,
      g_en_timer          => true,
      g_en_eca_tap        => true,
      g_en_asmi           => false,
      g_io_table          => io_mapping_table,
      g_lm32_cores        => c_cores,
      g_lm32_ramsizes     => c_lm32_ramsizes/4,
      g_lm32_init_files   => f_string_list_repeat(c_initf_name, c_cores),
      g_lm32_profiles     => f_string_list_repeat(c_profile_name, c_cores)
    )
    port map(
      core_clk_20m_vcxo_i    => clk_20m_vcxo_i,
      core_clk_125m_pllref_i => clk_125m_pllref_i,
      core_clk_125m_sfpref_i => clk_sfp_ref_i,
      core_clk_125m_local_i  => clk_125m_local_i,
      core_rstn_i            => fpga_res_i,
      core_clk_sys_o         => clk_sys,
      core_clk_200m_o        => clk_200m,
      core_clk_10m_o         => clk_10m,
      core_debug_o           => core_debug_out,

      wr_onewire_io          => rom_data_io,

      wr_sfp_sda_io          => sfp_mod2_io,
      wr_sfp_scl_io          => sfp_mod1_io,
      wr_sfp_det_i           => sfp_mod0_i,
      wr_sfp_tx_o            => sfp_txp_o,
      wr_sfp_rx_i            => sfp_rxp_i,
      wbar_phy_dis_o         => sfp_tx_dis_o,

      wr_dac_sclk_o          => wr_dac_sclk_o,
      wr_dac_din_o           => wr_dac_din_o,
      wr_ndac_cs_o           => wr_ndac_cs_o,
      wr_ext_clk_i           => clk_lvtio_i,

      sfp_tx_fault_i         => sfp_tx_fault_i,
      sfp_los_i              => sfp_los_i,

      gpio_o                 => s_gpio_out,
      gpio_i                 => s_gpio_in,

      lvds_p_i               => s_lvds_p_i(4 downto 0),
      lvds_n_i               => s_lvds_n_i(4 downto 0),
      lvds_i_led_o           => s_lvds_i_led(4 downto 0),

      lvds_p_o               => s_lvds_p_o(2 downto 0),
      lvds_n_o               => s_lvds_n_o(2 downto 0),
      lvds_o_led_o           => s_lvds_o_led(2 downto 0),
      lvds_oen_o             => s_lvds_oe(2 downto 0),
      lvds_term_o(2 downto 0) => s_lvds_term_en(2 downto 0),

      led_link_up_o          => s_led_link_up,
      led_link_act_o         => s_led_link_act,
      led_track_o            => s_led_track,
      led_pps_o              => s_led_pps,

      usb_rstn_o             => ures,
      usb_ebcyc_i            => pa(3),
      usb_speed_i            => pa(0),
      usb_shift_i            => pa(1),
      usb_readyn_io          => pa(7),
      usb_fifoadr_o          => pa(5 downto 4),
      usb_sloen_o            => pa(2),
      usb_fulln_i            => ctl(1),
      usb_emptyn_i           => ctl(2),
      usb_slrdn_o            => slrd,
      usb_slwrn_o            => slwr,
      usb_pktendn_o          => pa(6),
      usb_fd_io              => fd,

    -- g_en_lcd
      lcd_scp_o              => dis_di_o(3),
      lcd_lp_o               => dis_di_o(1),
      lcd_flm_o              => dis_di_o(2),
      lcd_in_o               => dis_di_o(0),

      pcie_refclk_i          => pcie_refclk_i,
      pcie_rstn_i            => nPCI_RESET,
      pcie_rx_i              => pcie_rx_i,
      pcie_tx_o              => pcie_tx_o

  );



  p_sync_aux: process(clk_sys)
  begin
    if rising_edge(clk_sys) then
        s_blink_counter <= s_blink_counter + 1;
    end if;
  end process;

  -- Special Output
  -- s_lvds_p_o(3)    <= s_led_pps;
  buffer_pps : altera_lvds_obuf
    generic map(
      g_family  => c_family)
    port map(
      datain    => s_led_pps,
      dataout   => s_lvds_p_o(3),
      dataout_b => s_lvds_n_o(3)
    );

  --s_lvds_p_o(4)    <= clk_10m;
  buffer_10mhz : altera_lvds_obuf
    generic map(
      g_family  => c_family)
    port map(
      datain    => clk_10m,
      dataout   => s_lvds_p_o(4),
      dataout_b => s_lvds_n_o(4)
    );

  s_lvds_o_led(3)   <= s_led_pps;
  s_lvds_o_led(4)   <= s_led_track;
  s_lvds_oe(3)      <= '1';
  s_lvds_oe(4)      <= '1';
  s_lvds_term_en(3) <= '0';
  s_lvds_term_en(4) <= '0';

  -- hex switches as gpio inputs
  s_gpio_in(3 downto 0) <= not hswf_i; -- FPGA HEX switch
  s_gpio_in(7 downto 4) <= con(4 downto 1); -- CPLD HEX switch

  -- button as gpio inputs
  s_gpio_in(8) <= not pbs_f_i; -- FPGA push button
  s_gpio_in(9) <= con(5);      -- CPLD push button


  -- test mode select via hex switch or sw
  -- invert FPGA button and HEX switch
  s_test_sel(4)          <= s_gpio_out(7)          when s_gpio_out(c_HWT_EN_BIT)='1' else not pbs_f_i;
  s_test_sel(3 downto 0) <= s_gpio_out(3 downto 0) when s_gpio_out(c_HWT_EN_BIT)='1' else not (hswf_i);


  -- Display
  dis_wr_o    <= '0';
  dis_rst_o   <= '1';

  -- WR status LEDs
  s_dis_led_green <= s_gpio_out(4) when s_gpio_out(8)='1' else (    s_led_link_up and     s_led_track); -- green
  s_dis_led_red   <= s_gpio_out(5) when s_gpio_out(8)='1' else (not s_led_link_up                    ); -- red
  s_dis_led_blue  <= s_gpio_out(6) when s_gpio_out(8)='1' else (    s_led_link_up and not s_led_track); -- blue

  -- display backlight color - pullups
  dis_di_o(4) <= '0' when s_dis_led_green = '1' else 'Z'; -- green
  dis_di_o(5) <= '0' when s_dis_led_red   = '1' else 'Z'; -- red
  dis_di_o(6) <= '0' when s_dis_led_blue  = '1' else 'Z'; -- blue

  -- WR Link status LEDs
  s_led_status_monster(4) <= s_led_link_act and s_led_link_up;   -- red   = traffic/no-link
  s_led_status_monster(1) <= s_led_link_up;                      -- blue  = link
  s_led_status_monster(5) <= s_led_track;                        -- green = timing valid
  s_led_status_monster(2) <= s_led_pps;                          -- white = PPS

  -- Front panels status LEDs
  s_led_status_monster(6) <= s_gpio_out(1); -- user LED 1 - RED
  s_led_status_monster(3) <= s_gpio_out(0); -- user LED 2 - BLUE


  -- status LED output according to FPGA hex switch position and fpga button
  -- F position - simple led test
  with s_test_sel select
    s_led_status <= "000000"                          when ('0' & x"F"),   -- FPGA hex sw in position F, button not pressed, led test - leds off
                    "111111"                          when ('1' & x"F"),   -- FPGA hex sw in position F, button     pressed, led test - leds on
                    s_shift_reg_to_leds(15 downto 10) when ('0' & x"A"),   -- FPGA hex sw in position A, button not pressed, shift reg to leds
                    s_led_status_monster              when others;         -- driven by monster

  -- Status LED index position on the front panel according to schematic and signal names
  --   ================================
  --   | [   ] 4  1               Labels on front panel: 4 -> Ac, 1 -> Li         1  2  [   ]
  --   | |SFP| 5  2  Lemos                               5 -> Lo, 2 -> PS   Lemos PS Lo [SFP]
  --   | [   ] 6  3                                      6 -> 2 , 3 -> 1          Li Ac [   ]
  --   --------------------------------                                     =================

  led_status_o <= not s_led_status;

  -- USER LED output according to fpga hex switch position and fpga button
  -- F position - simple led test
  -- D position - show state of CPLD hex switch and button
  with s_test_sel select
    s_led_user <= x"00"                       when ('0' & x"F"),   -- FPGA hex sw in position F, button not pressed, led test - leds off
                  x"FF"                       when ('1' & x"F"),   -- FPGA hex sw in position F, button     pressed, led test - leds on
                  ("000" &     con)           when ('0' & x"D"),   -- FPGA hex sw in position D, button not pressed, CPLD HEX SW and button test
                  ("000" & not con)           when ('1' & x"D"),   -- FPGA hex sw in position D, button     pressed, CPLD HEX SW and button test
                  core_debug_out( 7 downto 0) when ('0' & x"C"),   -- FPGA hex sw in position C, button not pressed, xwb control signals for pmc master output
                  core_debug_out(15 downto 8) when ('1' & x"C"),   -- FPGA hex sw in position C, button     pressed, xwb control signals for pmc master output
                  s_gpio_out(7 downto 0)      when others;         -- driven by monster

  led_user_o <= not s_led_user;



  -- enable LEMO output buffers (active LO)
  lvtio_oe_n_o <= not s_lvds_oe(4 downto 0);

  -- LEMO activity LEDs (active HI)
  s_lvds_led(4 downto 0) <= s_lvds_i_led(4 downto 0) or s_lvds_o_led(4 downto 0);

  -- LVDS termination pins (active hi)
  with s_test_sel select
    lvtio_term_en_o <= (others => '0')                 when ('0' & x"E"),   -- FPGA hex sw in position E, button not pressed, termination test
                       (others => '1')                 when ('1' & x"E"),   -- FPGA hex sw in position E, button     pressed, termination test
                        s_lvds_term_en(4 downto 0) when others;             -- driven by monster (enable termination when output disabled)

  -- LVDS direction indicator RED LEDs (active hi)
  with s_test_sel select
    lvtio_led_dir_o <= (others => '0')                  when ('0' & x"F"),   -- FPGA hex sw in position F, button not pressed, LED test
                       (others => '1')                  when ('1' & x"F"),   -- FPGA hex sw in position F, button     pressed, LED test
                        s_shift_reg_to_leds(9 downto 5) when ('0' & x"A"),   -- FPGA hex sw in position A, button not pressed, shift reg to leds
                        s_lvds_oe(4 downto 0)           when others;         -- driven by monster

  -- LVDS activity indicator BLUE LEDs (active hi)
  with s_test_sel select
    lvtio_led_act_o <= (others => '0')                  when ('0' & x"F"),   -- FPGA hex sw in position F, button not pressed, LED test
                       (others => '1')                  when ('1' & x"F"),   -- FPGA hex sw in position F, button     pressed, LED test
                        s_shift_reg_to_leds(4 downto 0) when ('0' & x"A"),   -- FPGA hex sw in position A, button not pressed, shift reg to leds
                        s_lvds_led(4 downto 0)          when others;         -- driven by monster

  -----------------------------------------------------------
  -- lemo io connectors on front panel to/from monster
  -----------------------------------------------------------
  -- lvds/lvttl lemos in/out
  s_lvds_p_i(4 downto 0) <= lvtio_in_p_i(5 downto 1);
  s_lvds_n_i(4 downto 0) <= lvtio_in_n_i(5 downto 1);

  lvtio_out_p_o(5 downto 1)   <= s_lvds_p_o(4 downto 0);
  lvtio_out_n_o(5 downto 1)   <= s_lvds_n_o(4 downto 0);


  -- External white rabbit clock input enable (active low)
  lvt_in_clk_en_n_o <= not(s_wr_ext_in);



  gen_load_shift_reg_true: if g_LOAD_SHIFT_REG_EN = true generate
    -----------------------------------------------------------------------
    -- TEST: Shift register to consume additiona resources not used by monster.
    --       Enabled by FPGA HexSwitch in position A
    -----------------------------------------------------------------------

    s_load_shift_en <= '1' when s_test_sel = ('0' & x"A") else '0';

    -- mega shift register to use up the rest of the resources not used by monster
    -- to put additional load on FPGA
    -- and register to hold values for leds
    p_load_shift_reg: process(clk_200m)
        -- maximal length 32-bit xnor LFSR based on xilinx app note XAPP210
        function lfsr32(x : std_logic_vector(31 downto 0)) return std_logic_vector is
        begin
            return x(30 downto 0) & (x(0) xnor x(1) xnor x(21) xnor x(31));
        end function;
    begin
      if rising_edge(clk_200m) then

        s_pseudo_rand_reg <= lfsr32(s_pseudo_rand_reg);

        for i in 0 to (c_LOAD_SHIFT_REG_WIDTH- 1) loop
          if s_load_shift_en = '1' then
            s_load_shift_reg_arr(i) <= s_load_shift_reg_arr(i)(c_LOAD_SHIFT_REG_DEPTH-2 downto 0) & s_pseudo_rand_reg(i);
          else
            s_load_shift_reg_arr(i) <= s_load_shift_reg_arr(i);
          end if;

          -- assign shift register output to logic analyzer port
          s_shift_reg_out(i)     <= s_load_shift_reg_arr(i)(c_LOAD_SHIFT_REG_DEPTH-1);

          if s_led_reg_en = '1' then
            s_shift_reg_to_leds(i) <= s_load_shift_reg_arr(i)(c_LOAD_SHIFT_REG_DEPTH-1) ;
          else
            s_shift_reg_to_leds(i) <= s_shift_reg_to_leds(i);
          end if;
        end loop;
      end if;
    end process;

    -- counter for latching shift register output to front panel leds for same indication of activity
    p_sync_aux: process(clk_200m)
    begin
      if rising_edge(clk_200m) then
        if s_led_reg_en = '1' then
          s_reg_blink_counter <= (others => '0');
        else
          s_reg_blink_counter <= s_reg_blink_counter + 1;
        end if;
      end if;
    end process;

    -- show shift reg output on front panel leds, set refresh rate with CPLD hex switch)
    s_led_reg_en <= '1' when ((to_integer(s_reg_blink_counter) >=  25_000_000 and con(4 downto 1) = x"0") or
                              (to_integer(s_reg_blink_counter) >=  50_000_000 and con(4 downto 1) = x"1") or
                              (to_integer(s_reg_blink_counter) >= 100_000_000 and con(4 downto 1) = x"2")
                             )
                     else '0';

    -- assign shift register output to logic analyzer port
    hpw <= s_shift_reg_out(hpw'range);

    s_log_in(15 downto 0) <= (others => '0');
    s_log_in(16)          <= '0';

  end generate; -- gen_load_shift_reg_true

  gen_load_shift_reg_false: if g_LOAD_SHIFT_REG_EN = false generate
    s_shift_reg_to_leds <= (others => '0');
    s_load_shift_en     <= '0';

    -- Logic analyzer
    -- inputs
    s_log_in(15 downto 0) <= hpw(15 downto 0);
    s_log_in(16)          <= hpwck;

    -- outputs
--    hpwck                 <= s_log_out(16) when s_log_oe(16) = '1' else 'Z';
--    hpw_out : for i in 0 to 15 generate
--      hpw(i)               <= s_log_out(i) when s_log_oe(i) = '1' else 'Z';
--    end generate;
    hpwck  <= clk_20m_vcxo_i;
    hpw(0) <= clk_125m_pllref_i;
    hpw(1) <= clk_125m_local_i;
    hpw(2) <= clk_sfp_ref_i;
    hpw(7 downto 3) <= con;
    hpw(12 downto 8) <= s_test_sel;
    hpw(13) <= con(5);
    hpw(14) <= s_test_sel(4);
    hpw(15) <= clk_20m_vcxo_i;
  end generate; -- gen_load_shift_reg_false

end rtl;
