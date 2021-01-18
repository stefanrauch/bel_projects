library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;  

entity testbench is
end entity;

architecture simulation of testbench is

    signal clk_20m_vcxo_i    : std_logic := '1';  -- 20MHz VCXO clock
    signal clk_125m_pllref_i : std_logic := '1';  -- 125 MHz PLL reference
    signal clk_125m_local_i  : std_logic := '1';  -- local clk from 125Mhz oszillator

    -----------------------------------------
    -- PCI express pins
    -----------------------------------------
    signal pcie_refclk_i  : std_logic := '1';
    signal pcie_rx_i      : std_logic_vector(3 downto 0) := (others => '0');
    signal pcie_tx_o      : std_logic_vector(3 downto 0);
    signal nPCI_RESET     : std_logic := '0';

    signal pe_smdat        : std_logic := 'Z'; -- !!!
    signal pe_snclk        : std_logic;   -- !!!
    signal pe_waken        : std_logic;   -- !!!

    ------------------------------------------------------------------------
    -- WR DAC signals
    ------------------------------------------------------------------------
    signal dac_sclk       : std_logic;
    signal dac_din        : std_logic;
    signal ndac_cs        : std_logic_vector(2 downto 1);

    -----------------------------------------------------------------------
    -- OneWire
    -----------------------------------------------------------------------
    signal rom_data        : std_logic := 'Z';

    -----------------------------------------------------------------------
    -- display
    -----------------------------------------------------------------------
    signal di              : std_logic_vector(6 downto 0);
    signal ai              : std_logic_vector(1 downto 0) := (others => '0');
    signal dout_LCD        : std_logic := '1';
    signal wrdis           : std_logic := '0';
    signal dres            : std_logic := '1';

    -----------------------------------------------------------------------
    -- io
    -----------------------------------------------------------------------
    signal fpga_res        : std_logic := '0';
    signal nres            : std_logic := '1';
    signal pbs2            : std_logic := '0';
    signal hpw             : inout std_logic_vector(15 downto 0) := (others => 'Z'); -- logic analyzer
    signal ant             : inout std_logic_vector(26 downto 1) := (others => 'Z'); -- trigger bus

    -----------------------------------------------------------------------
    -- pexaria5db1/2
    -----------------------------------------------------------------------
    signal p1              : inout std_logic := 'Z'; -- HPWX0 logic analyzer: 3.3V
    signal n1              : inout std_logic := 'Z'; -- HPWX1
    signal p2              : inout std_logic := 'Z'; -- HPWX2
    signal n2              : inout std_logic := 'Z'; -- HPWX3
    signal p3              : inout std_logic := 'Z'; -- HPWX4
    signal n3              : inout std_logic := 'Z'; -- HPWX5
    signal p4              : inout std_logic := 'Z'; -- HPWX6
    signal n4              : inout std_logic := 'Z'; -- HPWX7
    signal p5              : out   std_logic := 'Z'; -- LED1 1-6: 3.3V (red)   1|Z=off, 0=on
    signal n5              : out   std_logic := 'Z'; -- LED2           (blue)
    signal p6              : out   std_logic := 'Z'; -- LED3           (green)
    signal n6              : out   std_logic := 'Z'; -- LED4           (white)
    signal p7              : out   std_logic := 'Z'; -- LED5           (red)
    signal n7              : out   std_logic := 'Z'; -- LED6           (blue)
    signal p8              : out   std_logic := 'Z'; -- LED7 7-8: 2.5V (green)
    signal n8              : out   std_logic := 'Z'; -- LED8           (white)

    signal p9              : out   std_logic := 'Z'; -- TERMEN1 = terminate TTLIO1, 1=x, 0|Z=x (Q2 BSH103 -- G pin)
    signal n9              : out   std_logic := 'Z'; -- TERMEN2 = terminate TTLIO2, 1=x, 0|Z=x
    signal p10             : out   std_logic := 'Z'; -- TERMEN3 = terminate TTLIO3, 1=x, 0|Z=x
    signal n10             : out   std_logic := 'Z'; -- TTLEN1  = TTLIO1 output enable, 0=enable, 1|Z=disable
    signal p11             : out   std_logic := 'Z'; -- n/c
    signal n11             : out   std_logic := 'Z'; -- TTLEN3  = TTLIO2 output enable, 0=enable, 1|Z=disable
    signal p12             : out   std_logic := 'Z'; -- n/c
    signal n12             : out   std_logic := 'Z'; -- n/c
    signal p13             : out   std_logic := 'Z'; -- n/c
    signal n13             : out   std_logic := 'Z'; -- n/c
    signal p14             : out   std_logic := 'Z'; -- n/c
    signal n14             : out   std_logic := 'Z'; -- TTLEN5  = TTLIO3 output enable, 0=enable, 1|Z=disable
    signal p15             : out   std_logic := 'Z'; -- n/c
    signal n15             : inout std_logic := 'Z'; -- ROM_DATA
    signal p16             : out   std_logic := 'Z'; -- FPLED5  = TTLIO3 (red)  0=on, Z=off
    signal n16             : out   std_logic := 'Z'; -- FPLED6           (blue)

    signal p17             : in    std_logic := '0';        -- N_LVDS_1 / SYnIN
    signal n17             : in    std_logic := '1';        -- P_LVDS_1 / SYpIN
    signal p18             : in    std_logic := '0';        -- N_LVDS_2 / TRnIN
    signal n18             : in    std_logic := '1';        -- P_LVDS_2 / TRpIN
    signal p19             : out   std_logic;        -- N_LVDS_3 / CK200n
    signal --n19             : out   std_logic;        -- P_LVDS_3 / CK200p -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
    signal p21             : in    std_logic := '0';        -- N_LVDS_6  = TTLIO1 in
    signal n21             : in    std_logic := '1';        -- P_LVDS_6
    signal p22             : in    std_logic := '0';        -- N_LVDS_8  = TTLIO2 in
    signal n22             : in    std_logic := '1';        -- P_PVDS_8
    signal p23             : in    std_logic := '0';        -- N_LVDS_10 = TTLIO3 in
    signal n23             : in    std_logic := '1';        -- P_LVDS_10
    signal p24             : out   std_logic := '0';        -- N_LVDS_4 / SYnOU
    signal --n24             : out   std_logic;        -- P_LVDS_4 / SYpOU -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
    signal p25             : out   std_logic := '0';        -- N_LVDS_5  = TTLIO1 out
    signal n25             : out   std_logic := '1';        -- P_LVDS_5
    signal p26             : out   std_logic := 'Z'; -- FPLED3    = TTLIO2 (red)  0=on, Z=off
    signal n26             : out   std_logic := 'Z'; -- FPLED4             (blue)
    signal p27             : out   std_logic := '0';        -- N_LVDS_7  = TTLIO2 out
    signal n27             : out   std_logic := '1';        -- P_LVDS_7
    signal p28             : out   std_logic := '0';        -- N_LVDS_9  = TTLIO3 out
    signal n28             : out   std_logic := '1';        -- P_LVDS_9
    signal p29             : out   std_logic := 'Z'; -- FPLED1    = TTLIO1 (red)  0=on, Z=off
    signal n29             : out   std_logic := 'Z'; -- FPLED2             (blue)
    signal p30             : out   std_logic := 'Z'; -- n/c
    signal n30             : out   std_logic := 'Z'; -- n/c

    -----------------------------------------------------------------------
    -- connector cpld
    -----------------------------------------------------------------------
    signal con             : out std_logic_vector(5 downto 1);

    -----------------------------------------------------------------------
    -- usb
    -----------------------------------------------------------------------
    signal slrd            : out   std_logic;
    signal slwr            : out   std_logic;
    signal fd              : inout std_logic_vector(7 downto 0) := (others => 'Z');
    signal pa              : inout std_logic_vector(7 downto 0) := (others => 'Z');
    signal ctl             : in    std_logic_vector(2 downto 0) := (others => 'Z');
    signal uclk            : in    std_logic := '0';
    signal ures            : out   std_logic;

    -----------------------------------------------------------------------
    -- leds onboard
    -----------------------------------------------------------------------
    signal led             : out std_logic_vector(8 downto 1) := (others => '1');

    -----------------------------------------------------------------------
    -- leds SFPs
    -----------------------------------------------------------------------
    signal ledsfpr          : out std_logic_vector(4 downto 1);
    signal ledsfpg          : out std_logic_vector(4 downto 1);
    signal sfp234_ref_clk_i : in  std_logic := '1';

    -----------------------------------------------------------------------
    -- SFP1
    -----------------------------------------------------------------------

    signal sfp1_tx_disable_o : out std_logic := '0';
    signal sfp1_tx_fault     : in std_logic := '0';
    signal sfp1_los          : in std_logic := '0';

     --sfp1_txp_o        : out std_logic;
     --sfp1_rxp_i        : in  std_logic;

    signal sfp1_mod0         : in    std_logic := 'Z'; -- grounded by module
    signal sfp1_mod1         : inout std_logic := 'Z'; -- SCL
    signal sfp1_mod2         : inout std_logic := 'Z'; -- SDA

    -----------------------------------------------------------------------
    -- SFP2
    -----------------------------------------------------------------------

    signal sfp2_tx_disable_o : out std_logic := '0';
    signal sfp2_tx_fault     : in  std_logic := '0';
    signal sfp2_los          : in  std_logic := '0';

     --sfp2_txp_o        : out std_logic;
     --sfp2_rxp_i        : in  std_logic;

    signal sfp2_mod0         : in    std_logic := 'Z'; -- grounded by module
    signal sfp2_mod1         : inout std_logic := 'Z'; -- SCL
    signal sfp2_mod2         : inout std_logic := 'Z'; -- SDA

    -----------------------------------------------------------------------
    -- SFP3
    -----------------------------------------------------------------------

    signal sfp3_tx_disable_o : out std_logic := '0';
    signal sfp3_tx_fault     : in std_logic := '0';
    signal sfp3_los          : in std_logic := '0';

     --sfp3_txp_o        : out std_logic;
     --sfp3_rxp_i        : in  std_logic;

    signal sfp3_mod0         : in    std_logic := 'Z'; -- grounded by module
    signal sfp3_mod1         : inout std_logic := 'Z'; -- SCL
    signal sfp3_mod2         : inout std_logic := 'Z'; -- SDA

    -----------------------------------------------------------------------
    -- SFP4
    -----------------------------------------------------------------------

    signal sfp4_tx_disable_o : out std_logic := '0';
    signal sfp4_tx_fault     : in std_logic := '0';
    signal sfp4_los          : in std_logic := '0';

    signal sfp4_txp_o        : out std_logic;
    signal sfp4_rxp_i        : in  std_logic := '0';

    signal sfp4_mod0         : in    std_logic := 'Z'; -- grounded by module
    signal sfp4_mod1         : inout std_logic := 'Z'; -- SCL
    signal sfp4_mod2         : inout std_logic := 'Z'); -- SDA

begin


  --ez_usb_chip : if g_en_usb generate 
  ---- instance of EZUSB-chip 
  ---- this simulates the physical chip that is connected to the FPGA
  --chip : entity work.ez_usb_chip
  --  port map (
  --    rstn_i    => usb_rstn,
  --    ebcyc_o   => usb_ebcyc,
  --    readyn_o  => usb_readyn,
  --    fifoadr_i => usb_fifoadr,
  --    fulln_o   => usb_fulln,
  --    emptyn_o  => usb_emptyn,
  --    sloen_i   => usb_sloen,
  --    slrdn_i   => usb_slrdn,
  --    slwrn_i   => usb_slwrn,
  --    pktendn_i => usb_pktendn,
  --    fd_io     => usb_fd_io
  --    );
  --end generate;


  --wrex : entity work.wr_timing
  --port map(
  --  dac_hpll_load_p1_i => dac_hpll_load_p1,
  --  dac_hpll_data_i    => dac_hpll_data,
  --  dac_dpll_load_p1_i => dac_dpll_load_p1,
  --  dac_dpll_data_i    => dac_dpll_data,
  --  clk_ref_125_o      => clk_ref,
  --  clk_sys_62_5_o     => open,
  --  clk_dmtd_20_o      => clk_dmtd
  --);

  tr : entity work.pci_control 
  port map(
    clk_20m_vcxo_i    => clk_20m_vcxo_i,
    clk_125m_pllref_i => clk_125m_pllref_i,
    clk_125m_local_i  => clk_125m_local_i,

    -----------------------------------------
    -- PCI express pins
    -----------------------------------------
    pcie_refclk_i     => pcie_refclk_i,
    pcie_rx_i         => pcie_rx_i,
    pcie_tx_o         => pcie_tx_o,
    nPCI_RESET        => nPCI_RESET,

    pe_smdat          => pe_smdat,
    pe_snclk          => pe_snclk,
    pe_waken          => pe_waken,

    ------------------------------------------------------------------------
    -- WR DAC signals
    ------------------------------------------------------------------------
    dac_sclk          => dac_sclk,
    dac_din           => dac_din,
    ndac_cs           => ndac_cs,

    -----------------------------------------------------------------------
    -- OneWire
    -----------------------------------------------------------------------
    rom_data          => rom_data,

    -----------------------------------------------------------------------
    -- display
    -----------------------------------------------------------------------
    di                => di,
    ai                => ai,
    dout_LCD          => dout_LCD,
    wrdis             => wrdis,
    dres              => dres,

    -----------------------------------------------------------------------
    -- io
    -----------------------------------------------------------------------
    fpga_res          => fpga_res,
    nres              => nres,
    pbs2              => pbs2,
    hpw               => hpw,
    ant               => ant,

    -----------------------------------------------------------------------
    -- pexaria5db1/2
    -----------------------------------------------------------------------
    p1                => p1,
    n1                => n1,
    p2                => p2,
    n2                => n2,
    p3                => p3,
    n3                => n3,
    p4                => p4,
    n4                => n4,
    p5                => p5,
    n5                => n5,
    p6                => p6,
    n6                => n6,
    p7                => p7,
    n7                => n7,
    p8                => p8,
    n8                => n8,

    p9                => p9,
    n9                => n9,
    p10               => p10,
    n10               => n10,
    p11               => p11,
    n11               => n11,
    p12               => p12,
    n12               => n12,
    p13               => p13,
    n13               => n13,
    p14               => p14,
    n14               => n14,
    p15               => p15,
    n15               => n15,
    p16               => p16,
    n16               => n16,

    p17               => p17,
    n17               => n17,
    p18               => p18,
    n18               => n18,
    p19               => p19,
    --n19             : out   std_logic;        -- P_LVDS_3 / CK200p -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
    p21               => p21,
    n21               => n21,
    p22               => p22,
    n22               => n22,
    p23               => p23,
    n23               => n23,
    p24               => p24,
    --n24             : out   std_logic;        -- P_LVDS_4 / SYpOU -- NEEDED FOR SERDES(FPGA) TO LVDS BUFFER(BOARD)
    p25               => p25,
    n25               => n25,
    p26               => p26,
    n26               => n26,
    p27               => p27,
    n27               => n27,
    p28               => p28,
    n28               => n28,
    p29               => p29,
    n29               => n29,
    p30               => p30,
    n30               => n30,

    -----------------------------------------------------------------------
    -- connector cpld
    -----------------------------------------------------------------------
    con               => con,

    -----------------------------------------------------------------------
    -- usb
    -----------------------------------------------------------------------
    slrd              => slrd,
    slwr              => slwr,
    fd                => fd,
    pa                => pa,
    ctl               => ctl,
    uclk              => uclk,
    ures              => ures,

    -----------------------------------------------------------------------
    -- leds onboard
    -----------------------------------------------------------------------
    led               => led,

    -----------------------------------------------------------------------
    -- leds SFPs
    -----------------------------------------------------------------------
    ledsfpr            => ledsfpr,
    ledsfpg            => ledsfpg,
    sfp234_ref_clk_i   => sfp234_ref_clk_i,

    -----------------------------------------------------------------------
    -- SFP1
    -----------------------------------------------------------------------

    sfp1_tx_disable_o   => sfp1_tx_disable_o,
    sfp1_tx_fault       => sfp1_tx_fault,
    sfp1_los            => sfp1_los,

    --sfp1_txp_o        : out std_logic;
    --sfp1_rxp_i        : in  std_logic;

    sfp1_mod0           => sfp1_mod0,
    sfp1_mod1           => sfp1_mod1,
    sfp1_mod2           => sfp1_mod2,

    -----------------------------------------------------------------------
    -- SFP2
    -----------------------------------------------------------------------

    sfp2_tx_disable_o   => sfp2_tx_disable_o,
    sfp2_tx_fault       => sfp2_tx_fault,
    sfp2_los            => sfp2_los,

    --sfp2_txp_o        : out std_logic;
    --sfp2_rxp_i        : in  std_logic;

    sfp2_mod0           => sfp2_mod0,
    sfp2_mod1           => sfp2_mod1,
    sfp2_mod2           => sfp2_mod2,

    -----------------------------------------------------------------------
    -- SFP3
    -----------------------------------------------------------------------

    sfp3_tx_disable_o   => sfp3_tx_disable_o,
    sfp3_tx_fault       => sfp3_tx_fault,
    sfp3_los            => sfp3_los,

    --sfp3_txp_o        : out std_logic;
    --sfp3_rxp_i        : in  std_logic;

    sfp3_mod0           => sfp3_mod0,
    sfp3_mod1           => sfp3_mod1,
    sfp3_mod2           => sfp3_mod2,

    -----------------------------------------------------------------------
    -- SFP4
    -----------------------------------------------------------------------

    sfp4_tx_disable_o   => sfp4_tx_disable_o,
    sfp4_tx_fault       => sfp4_tx_fault,
    sfp4_los            => sfp4_los,

    sfp4_txp_o          => sfp4_txp_o,
    sfp4_rxp_i          => sfp4_rxp_i,

    sfp4_mod0           => sfp4_mod0,
    sfp4_mod1           => sfp4_mod1,
    sfp4_mod2           => sfp4_mod2
    )

end architecture;



