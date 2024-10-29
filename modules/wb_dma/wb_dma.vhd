library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;

entity wb_dma is
    -- generic (
    --   g_aux_phy_interface : boolean
    -- );
    port(
        clk_sys_i     : in std_logic;
        rstn_sys_i    : in std_logic;
    
        --master_o       : out t_wishbone_slave_out;
        --master_i       : in  t_wishnumeric_std-body.vhdlbone_slave_in;

    	-- test signals
        s_start_desc_i        : in std_logic;
        s_read_init_address_i : in t_wishbone_address;
        s_descriptor_active_i : in std_logic;
        s_data_in_i           : in std_logic_vector(c_wishbone_data_width-1 downto 0);

        s_data_in_ack_i       : in std_logic
        );
end entity;

architecture rtl of wb_dma is

    -- DMA ENGINE SIGNALS
    ------------------------------------------
    -- read ops signals
    signal s_read_ops_we             : std_logic := '0';

    signal s_load_descriptor_en      : std_logic := '0';

    signal s_desc_csr_sz_we  : in std_logic;
    signal s_desc_addr0_we   : in std_logic;
    signal s_desc_addr1      : in std_logic;
    signal s_pointer_we      : in std_logic;

    -- read logic
    signal s_ops_queue_full          : std_logic := '0';
    signal s_ops_queue_empty         : std_logic := '1';
    signal s_read_enable             : std_logic := '0';
    signal s_store_read_op           : std_logic_vector(2*c_wishbone_address_width downto 0) := (others => '0');
    signal s_data_cache_write_enable : std_logic := '0';

    -- REGISTER FILE SIGNALS
    ------------------------------------------
    signal s_read_ops_read_en     : std_logic := '0';
    signal s_read_ops             : std_logic_vector((2*c_wishbone_address_width)-1 downto 0) := (others => '0');
    signal s_read_ops_cache_full  : std_logic := '0';
    signal s_read_ops_cache_empty : std_logic := '1';
    signal s_read_init_address    : std_logic_vector(c_wishbone_address_width-1 downto 0) := (others => '0');

    -- read FSM signals
    signal cache_we          : std_logic := '0'; -- write enable
    signal data_cache_empty  : std_logic := '1';

    -- write FSM signals
    signal cache_re          : std_logic := '0'; -- read enable
    signal data_cache_full   : std_logic := '0';

  component wb_dma_engine is
  port(
    clk_i : in std_logic;
    rstn_i : in std_logic;

    s_load_descriptor_en_o      : out std_logic;

    s_desc_csr_sz_we_i  : in std_logic;
    s_desc_addr0_we_i   : in std_logic;
    s_desc_addr1_i      : in std_logic;
    s_pointer_we_i      : in std_logic;

    -- read logic
    s_queue_full_i              : in std_logic;
    s_queue_empty_i             : in std_logic;
    s_read_enable_o             : out std_logic := '0';
    s_data_cache_write_enable_o : out std_logic := '0';
    s_read_ack                  : in std_logic;

    --only for testing!!!!
    s_start_desc                : in std_logic;
    s_read_init_address         : in std_logic_vector(c_wishbone_address_width-1 downto 0);
    s_descriptor_active         : in std_logic;

    s_data_in_ack_i             : in std_logic
  );
  end component;

  component wb_dma_ch_rf is
    generic(
      g_data_cache_size : natural := 16
    );
    port(
      clk_i : in std_logic;
      rstn_i : in std_logic;

      -- module IOs
      --wb_data_i : in std_logic_vector(c_wishbone_data_width-1 downto 0);
      --wb_data_o : out std_logic_vector(c_wishbone_data_width-1 downto 0);

      -- control signals
      s_desc_csr_sz_we  : in std_logic;
      s_desc_addr0_we   : in std_logic;
      s_desc_addr1      : in std_logic;
      s_pointer_we      : in std_logic;

      data_in           : in std_logic_vector(c_wishbone_data_width-1 downto 0);
  
      -- read FSM signals
      cache_we          : in std_logic; -- write enable
      data_cache_empty  : out std_logic;
  
      -- write FSM signals
      cache_re          : in std_logic; -- read enable
      data_cache_full   : out std_logic
    );
  end component;
    

begin

    dma_engine: wb_dma_engine
    port map (
        clk_i => clk_sys_i,
        rstn_i => rstn_sys_i,

        s_load_descriptor_en_o => s_load_descriptor_en,

        s_desc_csr_sz_we_i  => s_desc_csr_sz_we,
        s_desc_addr0_we_i   => s_desc_addr0_we,
        s_desc_addr1_i      => s_desc_addr1,
        s_pointer_we_i      => s_pointer_we,

        -- read logic
        s_queue_full_i => s_ops_queue_full,
        s_queue_empty_i => s_ops_queue_empty,
        s_read_enable_o => s_read_enable,
        s_data_cache_write_enable_o => s_data_cache_write_enable,
        s_read_ack => '0',
        
        s_start_desc => s_start_desc_i,
        s_read_init_address => s_read_init_address_i,
        s_descriptor_active => s_descriptor_active_i,

        s_data_in_ack_i => s_data_in_ack_i
    );    

    register_file: wb_dma_ch_rf
    generic map (
        g_data_cache_size => 16
    )
    port map (
      clk_i => clk_sys_i,
      rstn_i => rstn_sys_i,
  
      -- module IOs
      --wb_data_i => (others => '0'), 
      --wb_data_o => open, 
      -- read FSM signals

      s_desc_csr_sz_we  => s_desc_csr_sz_we,
      s_desc_addr0_we   => s_desc_addr0_we,
      s_desc_addr1      => s_desc_addr1,
      s_pointer_we      => s_pointer_we,

      data_in => s_data_in_i,

      cache_we          => '0',
      data_cache_empty  => open,
  
      -- write FSM signals
      cache_re          => '0',
      data_cache_full   => open
    );
end architecture;
