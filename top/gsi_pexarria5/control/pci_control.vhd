library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.monster_pkg.all;
use work.altera_lvds_pkg.all;
use work.ramsize_pkg.c_lm32_ramsizes;
use work.altera_networks_pkg.all;

entity pci_control is
  port(
    clk_20m_vcxo_i    : in std_logic;  -- 20MHz VCXO clock
    clk_125m_pllref_i : in std_logic;  -- 125 MHz PLL reference
    clk_125m_local_i  : in std_logic;  -- local clk from 125Mhz oszillator

    -----------------------------------------
    -- PCI express pins
    -----------------------------------------
    pcie_refclk_i  : in  std_logic;
    pcie_rx_i      : in  std_logic_vector(3 downto 0);
    pcie_tx_o      : out std_logic_vector(3 downto 0);
    nPCI_RESET     : in std_logic;

    pe_smdat        : inout std_logic; -- !!!
    pe_snclk        : out std_logic;   -- !!!
    pe_waken        : out std_logic;   -- !!!

    ------------------------------------------------------------------------
    -- WR DAC signals
    ------------------------------------------------------------------------
    dac_sclk       : out std_logic;
    dac_din        : out std_logic;
    ndac_cs        : out std_logic_vector(2 downto 1);

    -----------------------------------------------------------------------
    -- OneWire
    -----------------------------------------------------------------------
    rom_data        : inout std_logic;

    -----------------------------------------------------------------------
    -- display
    -----------------------------------------------------------------------
    di              : out std_logic_vector(6 downto 0);
    ai              : in  std_logic_vector(1 downto 0);
    dout_LCD        : in  std_logic;
    wrdis           : out std_logic := '0';
    dres            : out std_logic := '1';

    -----------------------------------------------------------------------
    -- io
    -----------------------------------------------------------------------
    fpga_res        : in std_logic;
    nres            : in std_logic;
    pbs2            : in std_logic;
    hpw             : inout std_logic_vector(15 downto 0) := (others => 'Z'); -- logic analyzer
    ant             : inout std_logic_vector(26 downto 1) := (others => 'Z'); -- trigger bus

    -----------------------------------------------------------------------
    -- pexaria5db1/2
    -----------------------------------------------------------------------
    p1              : inout std_logic := 'Z'; -- HPWX0 logic analyzer: 3.3V
    n1              : inout std_logic := 'Z'; -- HPWX1
    p2              : inout std_logic := 'Z'; -- HPWX2
    n2              : inout std_logic := 'Z'; -- HPWX3
    p3              : inout std_logic := 'Z'; -- HPWX4
    n3              : inout std_logic := 'Z'; -- HPWX5
    p4              : inout std_logic := 'Z'; -- HPWX6
    n4              : inout std_logic := 'Z'; -- HPWX7
    p5              : out   std_logic := 'Z'; -- LED1 1-6: 3.3V (red)   1|Z=off, 0=on
    n5              : out   std_logic := 'Z'; -- LED2           (blue)
    p6              : out   std_logic := 'Z'; -- LED3           (green)
    n6              : out   std_logic := 'Z'; -- LED4           (white)
    p7              : out   std_logic := 'Z'; -- LED5           (red)
    n7              : out   std_logic := 'Z'; -- LED6           (blue)
    p8              : out   std_logic := 'Z'; -- LED7 7-8: 2.5V (green)
    n8              : out   std_logic := 'Z'; -- LED8           (white)

    p9              : out   std_logic := 'Z'; -- TERMEN1 = terminate TTLIO1, 1=x, 0|Z=x (Q2 BSH103 -- G pin)
    n9              : out   std_logic := 'Z'; -- TERMEN2 = terminate TTLIO2, 1=x, 0|Z=x
    p10             : out   std_logic := 'Z'; -- TERMEN3 = terminate TTLIO3, 1=x, 0|Z=x
    n10             : out   std_logic := 'Z'; -- TTLEN1  = TTLIO1 output enable, 0=enable, 1|Z=disable
    p11             : out   std_logic := 'Z'; -- n/c
    n11             : out   std_logic := 'Z'; -- TTLEN3  = TTLIO2 output enable, 0=enable, 1|Z=disable
    p12             : out   std_logic := 'Z'; -- n/c
    n12             : out   std_logic := 'Z'; -- n/c
    p13             : out   std_logic := 'Z'; -- n/c
    n13             : out   std_logic := 'Z'; -- n/c
    p14             : out   std_logic := 'Z'; -- n/c
    n14             : out   std_logic := 'Z'; -- TTLEN5  = TTLIO3 output enable, 0=enable, 1|Z=disable
    p15             : out   std_logic := 'Z'; -- n/c
    n15             : inout std_logic := 'Z'; -- ROM_DATA
    p16             : out   std_logic := 'Z'; -- FPLED5  = TTLIO3 (red)  0=on, Z=off
    n16             : out   std_logic := 'Z'; -- FPLED6           (blue)

    p17             : in    std_logic;        -- N_LVDS_1 / SYnIN
    n17             : in    std_logic;        -- P_LVDS_1 / SYpIN
    p18             : in    std_logic;        -- N_LVDS_2 / TRnIN
    n18             : in    std_logic;        -- P_LVDS_2 / TRpIN
    p19             : out   std_logic;        -- N_LVDS_3 / CK200n
    --n19             : out   std_logic;        -- P_LVDS_3 / CK200p -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
    p21             : in    std_logic;        -- N_LVDS_6  = TTLIO1 in
    n21             : in    std_logic;        -- P_LVDS_6
    p22             : in    std_logic;        -- N_LVDS_8  = TTLIO2 in
    n22             : in    std_logic;        -- P_PVDS_8
    p23             : in    std_logic;        -- N_LVDS_10 = TTLIO3 in
    n23             : in    std_logic;        -- P_LVDS_10
    p24             : out   std_logic;        -- N_LVDS_4 / SYnOU
    --n24             : out   std_logic;        -- P_LVDS_4 / SYpOU -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
    p25             : out   std_logic;        -- N_LVDS_5  = TTLIO1 out
    n25             : out   std_logic;        -- P_LVDS_5
    p26             : out   std_logic := 'Z'; -- FPLED3    = TTLIO2 (red)  0=on, Z=off
    n26             : out   std_logic := 'Z'; -- FPLED4             (blue)
    p27             : out   std_logic;        -- N_LVDS_7  = TTLIO2 out
    n27             : out   std_logic;        -- P_LVDS_7
    p28             : out   std_logic;        -- N_LVDS_9  = TTLIO3 out
    n28             : out   std_logic;        -- P_LVDS_9
    p29             : out   std_logic := 'Z'; -- FPLED1    = TTLIO1 (red)  0=on, Z=off
    n29             : out   std_logic := 'Z'; -- FPLED2             (blue)
    p30             : out   std_logic := 'Z'; -- n/c
    n30             : out   std_logic := 'Z'; -- n/c

    -----------------------------------------------------------------------
    -- connector cpld
    -----------------------------------------------------------------------
    con             : out std_logic_vector(5 downto 1);

    -----------------------------------------------------------------------
    -- usb
    -----------------------------------------------------------------------
    slrd            : out   std_logic;
    slwr            : out   std_logic;
    fd              : inout std_logic_vector(7 downto 0) := (others => 'Z');
    pa              : inout std_logic_vector(7 downto 0) := (others => 'Z');
    ctl             : in    std_logic_vector(2 downto 0);
    uclk            : in    std_logic;
    ures            : out   std_logic;

    -----------------------------------------------------------------------
    -- leds onboard
    -----------------------------------------------------------------------
    led             : out std_logic_vector(8 downto 1) := (others => '1');

    -----------------------------------------------------------------------
    -- leds SFPs
    -----------------------------------------------------------------------
    ledsfpr          : out std_logic_vector(4 downto 1);
    ledsfpg          : out std_logic_vector(4 downto 1);
    sfp234_ref_clk_i : in  std_logic;

    -----------------------------------------------------------------------
    -- SFP1
    -----------------------------------------------------------------------

    sfp1_tx_disable_o : out std_logic := '0';
    sfp1_tx_fault     : in std_logic;
    sfp1_los          : in std_logic;

    --sfp1_txp_o        : out std_logic;
    --sfp1_rxp_i        : in  std_logic;

    sfp1_mod0         : in    std_logic; -- grounded by module
    sfp1_mod1         : inout std_logic; -- SCL
    sfp1_mod2         : inout std_logic; -- SDA

    -----------------------------------------------------------------------
    -- SFP2
    -----------------------------------------------------------------------

    sfp2_tx_disable_o : out std_logic := '0';
    sfp2_tx_fault     : in  std_logic;
    sfp2_los          : in  std_logic;

    --sfp2_txp_o        : out std_logic;
    --sfp2_rxp_i        : in  std_logic;

    sfp2_mod0         : in    std_logic; -- grounded by module
    sfp2_mod1         : inout std_logic; -- SCL
    sfp2_mod2         : inout std_logic; -- SDA

    -----------------------------------------------------------------------
    -- SFP3
    -----------------------------------------------------------------------

    sfp3_tx_disable_o : out std_logic := '0';
    sfp3_tx_fault     : in std_logic;
    sfp3_los          : in std_logic;

    --sfp3_txp_o        : out std_logic;
    --sfp3_rxp_i        : in  std_logic;

    sfp3_mod0         : in    std_logic; -- grounded by module
    sfp3_mod1         : inout std_logic; -- SCL
    sfp3_mod2         : inout std_logic; -- SDA

    -----------------------------------------------------------------------
    -- SFP4
    -----------------------------------------------------------------------

    sfp4_tx_disable_o : out std_logic := '0';
    sfp4_tx_fault     : in std_logic;
    sfp4_los          : in std_logic;

    sfp4_txp_o        : out std_logic;
    sfp4_rxp_i        : in  std_logic;

    sfp4_mod0         : in    std_logic; -- grounded by module
    sfp4_mod1         : inout std_logic; -- SCL
    sfp4_mod2         : inout std_logic); -- SDA
end pci_control;

architecture rtl of pci_control is

  signal led_link_up   : std_logic;
  signal led_link_act  : std_logic;
  signal led_track     : std_logic;
  signal led_pps       : std_logic;

  signal gpio_o        : std_logic_vector(7 downto 0);
  signal lvds_p_i      : std_logic_vector(4 downto 0);
  signal lvds_n_i      : std_logic_vector(4 downto 0);
  signal lvds_i_led    : std_logic_vector(4 downto 0);
  signal lvds_p_o      : std_logic_vector(0 downto 0);
  signal lvds_n_o      : std_logic_vector(0 downto 0);
  signal lvds_o_led    : std_logic_vector(0 downto 0);
  signal lvds_oen      : std_logic_vector(0 downto 0);
  signal lvds_term     : std_logic_vector(0 downto 0);

  signal butis_clk_200 : std_logic;
  signal butis_t0_ts   : std_logic;
  signal clk_10m       : std_logic;

  constant io_mapping_table : t_io_mapping_table_arg_array(0 to 16) :=
  (
  -- Name[12 Bytes], Special Purpose, SpecOut, SpecIn, Index, Direction,   Channel,  OutputEnable, Termination, Logic Level
    ("LED1_BASE_R", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("LED2_BASE_B", IO_NONE,         false,   false,  1,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("LED3_BASE_G", IO_NONE,         false,   false,  2,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("LED4_BASE_W", IO_NONE,         false,   false,  3,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("LED1_ADD_R ", IO_NONE,         false,   false,  4,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("LED2_ADD_B ", IO_NONE,         false,   false,  5,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("LED3_ADD_G ", IO_NONE,         false,   false,  6,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("LED4_ADD_W ", IO_NONE,         false,   false,  7,     IO_OUTPUT,   IO_GPIO,  false,        false,       IO_TTL),
    ("IO1        ", IO_NONE,         false,   false,  0,     IO_INOUTPUT, IO_LVDS,  true,         true,        IO_LVTTL),
    ("IO2        ", IO_NONE,         false,   false,  1,     IO_INPUT,    IO_LVDS,  false,        false,       IO_LVTTL),
    ("IO3        ", IO_NONE,         false,   false,  2,     IO_INPUT,    IO_LVDS,  false,        false,       IO_LVTTL),
    ("MHDMR_SYIN ", IO_NONE,         false,   false,  3,     IO_INPUT,    IO_LVDS,  false,        false,       IO_LVDS),
    ("MHDMR_TRIN ", IO_NONE,         false,   false,  4,     IO_INPUT,    IO_LVDS,  false,        false,       IO_LVDS),
    ("MHDMR_CK200", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_FIXED, false,        false,       IO_LVDS),
    ("MHDMR_SYOU ", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_FIXED, false,        false,       IO_LVDS),
    ("IO2_PPS    ", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_FIXED, false,        false,       IO_LVTTL),
    ("IO3_10MHZ  ", IO_NONE,         false,   false,  0,     IO_OUTPUT,   IO_FIXED, false,        false,       IO_LVTTL)
  );
  constant c_family       : string := "Arria V";
  constant c_project      : string := "pci_control";
  constant c_initf_name   : string := c_project & "_stub.mif";
  constant c_profile_name : string := "medium_icache_debug";
  constant c_cores        : natural := 1;

begin

  main : monster
    generic map(
      g_family            => c_family,
      g_project           => c_project,
      g_flash_bits        => 25,
      g_gpio_out          => 8,
      g_lvds_in           => 4,
      g_lvds_out          => 0,
      g_lvds_inout        => 1,
      g_fixed             => 4,
      g_lvds_invert       => true,
      g_en_pcie           => true,
      g_en_usb            => true,
      g_en_lcd            => true,
      g_en_user_ow        => true,
      g_en_tempsens       => true,
      g_delay_diagnostics => true,
      g_en_timer          => true,
      g_en_eca_tap        => true,
      g_io_table          => io_mapping_table,
      g_lm32_cores        => c_cores,
      g_lm32_ramsizes     => c_lm32_ramsizes/4,
      g_lm32_init_files   => f_string_list_repeat(c_initf_name, c_cores),
      g_lm32_profiles     => f_string_list_repeat(c_profile_name, c_cores),
      g_en_asmi           => false
    )
    port map(
      core_clk_20m_vcxo_i     => clk_20m_vcxo_i,
      core_clk_125m_pllref_i  => clk_125m_pllref_i,
      core_clk_125m_sfpref_i  => sfp234_ref_clk_i,
      core_clk_125m_local_i   => clk_125m_local_i,
      core_rstn_i             => pbs2,
      core_clk_butis_o        => butis_clk_200,
      core_clk_butis_t0_o     => butis_t0_ts,
      core_clk_10m_o          => clk_10m,
      wr_onewire_io           => rom_data,
      wr_sfp_sda_io           => sfp4_mod2,
      wr_sfp_scl_io           => sfp4_mod1,
      wr_sfp_det_i            => sfp4_mod0,
      wr_sfp_tx_o             => sfp4_txp_o,
      wr_sfp_rx_i             => sfp4_rxp_i,
      wbar_phy_dis_o          => sfp4_tx_disable_o,
      wr_dac_sclk_o           => dac_sclk,
      wr_dac_din_o            => dac_din,
      wr_ndac_cs_o            => ndac_cs,
      sfp_tx_disable_o        => open,
      sfp_tx_fault_i          => sfp4_tx_fault,
      sfp_los_i               => sfp4_los,
      gpio_o                  => gpio_o,
      lvds_p_i                => lvds_p_i,
      lvds_n_i                => lvds_n_i,
      lvds_i_led_o            => lvds_i_led,
      lvds_p_o                => lvds_p_o,
      lvds_n_o                => lvds_n_o,
      lvds_o_led_o            => lvds_o_led,
      lvds_oen_o              => lvds_oen,
      lvds_term_o(0)          => lvds_term(0),
      led_link_up_o           => led_link_up,
      led_link_act_o          => led_link_act,
      led_track_o             => led_track,
      led_pps_o               => led_pps,
      pcie_refclk_i           => pcie_refclk_i,
      pcie_rstn_i             => nPCI_RESET,
      pcie_rx_i               => pcie_rx_i,
      pcie_tx_o               => pcie_tx_o,
      usb_rstn_o              => ures,
      usb_ebcyc_i             => pa(3),
      usb_speed_i             => pa(0),
      usb_shift_i             => pa(1),
      usb_readyn_io           => pa(7),
      usb_fifoadr_o           => pa(5 downto 4),
      usb_sloen_o             => pa(2),
      usb_fulln_i             => ctl(1),
      usb_emptyn_i            => ctl(2),
      usb_slrdn_o             => slrd,
      usb_slwrn_o             => slwr,
      usb_pktendn_o           => pa(6),
      usb_fd_io               => fd,
      ow_io(0)                => n15,
      ow_io(1)                => 'Z',
      lcd_scp_o               => di(3),
      lcd_lp_o                => di(1),
      lcd_flm_o               => di(2),
      lcd_in_o                => di(0));

  -- SFP1-3 are not mounted
  sfp1_tx_disable_o <= '1';
  sfp2_tx_disable_o <= '1';
  sfp3_tx_disable_o <= '1';

  -- Link LEDs
  wrdis <= '0';
  dres  <= '1';
  di(5) <= '0' when (not led_link_up)                   = '1' else 'Z'; -- red
  di(6) <= '0' when (    led_link_up and not led_track) = '1' else 'Z'; -- blue
  di(4) <= '0' when (    led_link_up and     led_track) = '1' else 'Z'; -- green

  led(1) <= not (led_link_act and led_link_up); -- red   = traffic/no-link
  led(2) <= not led_link_up;                    -- blue  = link
  led(3) <= not led_track;                      -- green = timing valid
  led(4) <= not led_pps;                        -- white = PPS

  ledsfpg(3 downto 1) <= (others => '1');
  ledsfpr(3 downto 1) <= (others => '1');
  ledsfpg(4) <= not led_link_up;
  ledsfpr(4) <= not led_link_act;

  -- GPIO LEDs
  led(5) <= '0' when gpio_o(0)='1' else 'Z'; -- (baseboard), red
  led(6) <= '0' when gpio_o(1)='1' else 'Z'; -- blue
  led(7) <= '0' when gpio_o(2)='1' else 'Z'; -- green
  led(8) <= '0' when gpio_o(3)='1' else 'Z'; -- white
  p7     <= '0' when gpio_o(4)='1' else 'Z'; -- (add-on board), red
  n7     <= '0' when gpio_o(5)='1' else 'Z'; -- blue
  p8     <= '0' when gpio_o(6)='1' else 'Z'; -- green
  n8     <= '0' when gpio_o(7)='1' else 'Z'; -- white

  -- BuTiS/MDMHR Output
  p19 <= butis_clk_200;
  p24 <= not(butis_t0_ts);

  -- BuTiS/MHDMR activity LEDs
  p6  <= '0' when butis_clk_200='1' else 'Z'; -- LED3 (near HDMI = CK200 / LVDS3)
  n6  <= '0' when butis_t0_ts='1'   else 'Z'; -- LED4 (near HDMI = SYOU  / LVDS4)

  -- LVDS->LEMO output enable / termination
  n10 <= '0' when lvds_oen(0)='1' else 'Z'; -- TTLIO1 output enable
  --n11 <= '0' when lvds_oen(1)='1' else 'Z'; -- TTLIO2 output enable
  --n14 <= '0' when lvds_oen(2)='1' else 'Z'; -- TTLIO3 output enable
  n11 <= '0';
  n14 <= '0';

  p9  <= '1' when lvds_term(0)='1' else '0'; -- TERMEN1 (terminate when input)
  --n9  <= '1' when lvds_term(1)='1' else '0'; -- TERMEN2 (terminate when input)
  --p10 <= '1' when lvds_term(2)='1' else '0'; -- TERMEN3 (terminate when input)
  n9  <= '0';
  p10 <= '0';

  p29 <= '0' when lvds_oen(0)='1' else 'Z'; -- FPLED1/TTLIO1 red
  --p26 <= '0' when lvds_oen(1)='1' else 'Z'; -- FPLED3/TTLIO2 red
  --p16 <= '0' when lvds_oen(2)='1' else 'Z'; -- FPLED5/TTLIO3 red
  p26 <= '0';
  p16 <= '0';

  -- LVDS inputs
  lvds_p_i(0) <= p21; -- TTLIO1
  lvds_p_i(1) <= p22; -- TTLIO2
  lvds_p_i(2) <= p23; -- TTLIO3
  lvds_p_i(3) <= p17; -- LVDS_1 / SYIN
  lvds_p_i(4) <= p18; -- LVDS_2 / TRIN
  lvds_n_i(0) <= n21; -- TTLIO1
  lvds_n_i(1) <= n22; -- TTLIO2
  lvds_n_i(2) <= n23; -- TTLIO3
  lvds_n_i(3) <= n17; -- LVDS_1 / SYIN
  lvds_n_i(4) <= n18; -- LVDS_2 / TRIN

  -- LVDS outputs
  n25 <= lvds_n_o(0); -- TTLIO1
  --n27 <= lvds_n_o(1); -- TTLIO2
  --n28 <= lvds_n_o(2); -- TTLIO3
  --n19 <= lvds_n_o(3); -- LVDS_3 / CK200 -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
  --n24 <= lvds_n_o(4); -- LVDS_4 / SYOU  -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
  p25 <= lvds_p_o(0); -- TTLIO1
  --p27 <= lvds_p_o(1); -- TTLIO2
  --p28 <= lvds_p_o(2); -- TTLIO3
  --p19 <= lvds_p_o(3); -- LVDS_3 / CK200 -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
  --p24 <= lvds_p_o(4); -- LVDS_4 / SYOU  -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)

  -- Special Output
  -- s_lvds_p_o(3)    <= s_led_pps;
  buffer_pps : altera_lvds_obuf
    generic map(
      g_family  => c_family)
    port map(
      datain    => led_pps,
      dataout   => p27 ,
      dataout_b => n27
    );

  --s_lvds_p_o(4)    <= clk_10m;
  buffer_10mhz : altera_lvds_obuf
    generic map(
      g_family  => c_family)
    port map(
      datain    => clk_10m,
      dataout   => p28,
      dataout_b => n28
    );

  -- LVDS activity LEDs
  n29 <= '0' when lvds_i_led(0)='1' else 'Z'; -- FPLED2/TTLIO1 blue
  --n26 <= '0' when lvds_i_led(1)='1' else 'Z'; -- FPLED4/TTLIO2 blue
  --n16 <= '0' when lvds_i_led(2)='1' else 'Z'; -- FPLED6/TTLIO3 blue
  n26 <= led_pps;
  n16 <= led_track;
  p5  <= '0' when lvds_i_led(3)='1' else 'Z'; -- LED1 (near HDMI = SYIN  / LVDS1)
  n5  <= '0' when lvds_i_led(4)='1' else 'Z'; -- LED2 (near HDMI = TRIN  / LVDS2)
  --p6  <= '0' when lvds_o_led(3)='1' else 'Z'; -- LED3 (near HDMI = CK200 / LVDS3) -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
  --n6  <= '0' when lvds_o_led(4)='1' else 'Z'; -- LED4 (near HDMI = SYOU  / LVDS4) -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)

  -- Wires to CPLD, currently unused
  con <= (others => 'Z');

end rtl;
