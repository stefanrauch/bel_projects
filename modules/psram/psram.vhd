------------------------------------------------------------------------------
-- Title      : Wishbone pSRAM / cellular RAM core
-- Project    : General Cores
------------------------------------------------------------------------------
-- File       : psram.vhd
-- Author     : Wesley W. Terpstra
-- Company    : GSI
-- Created    : 2014-12-05
-- Last update: 2014-12-05
-- Platform   : FPGA-generic
-- Standard   : VHDL'93
-------------------------------------------------------------------------------
-- Description: Maps a pSRAM chip to wishbone memory
-------------------------------------------------------------------------------
-- Copyright (c) 2013 GSI
-------------------------------------------------------------------------------
-- Revisions  :
-- Date        Version  Author          Description
-- 2014-12-05  1.0      terpstra        Created
-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;

entity psram is
  generic(
    g_bits     : natural := 24;
    g_row_bits : natural := 8);
  port(
    clk_i     : in    std_logic; -- Must be <= 66MHz, modify c_bcr_* otherwise
    rstn_i    : in    std_logic;
    slave_i   : in    t_wishbone_slave_in;
    slave_o   : out   t_wishbone_slave_out;
    ps_clk    : out   std_logic;
    ps_addr   : out   std_logic_vector(g_bits-1 downto 0);
    ps_data   : inout std_logic_vector(15 downto 0);
    ps_seln   : out   std_logic_vector(1 downto 0);
    ps_cen    : out   std_logic;
    ps_oen    : out   std_logic;
    ps_wen    : out   std_logic;
    ps_cre    : out   std_logic;
    ps_advn   : out   std_logic;
    ps_wait   : in    std_logic);
end entity;

-- !!! does not respect tCEM burst-limit

architecture rtl of psram is

  -- Bus Control Register configuration:
  --  xx-20   0 reserved
  --  19-18  10 Bus Control Register
  --  17-16  00 reserved
  --  15      0 synchronous burst access mode
  --  14      0 variable latency
  --  13-11 010 latency = 3 clocks
  --  10      0 active low wait (need pull-up and 1=valid data)
  --   9      0 reserved
  --   8      1 wait arrives one cycle before data transfer
  --   7-6   00 reserved
  --   5-4   01 50% drive strength
  --   3      0 burst wrap-around
  --   2-0  111 continuous burst
  constant c_bcr_setup : std_logic_vector(ps_addr'range) :=
    (0 => '1', 1 => '1', 2 => '1', 4 => '1', 8 => '1', 12 => '1', 19 => '1',
     others => '0');
  constant c_bcr_time : natural := 5; -- at least 70ns in clocks

  type t_state is (S_RESET, S_IDLE, S_WRITE_REQUEST, S_WRITE_WAIT, S_WRITE_LATCH, S_READ_REQUEST, S_READ_WAIT, S_READ_LATCH, S_BCR_REQUEST, S_BCR_WAIT, S_BCR_GAP, S_BCR_READ);

  -- PSRAM input path
  signal r_state   : t_state := S_RESET;
  signal r_count   : unsigned(f_ceil_log2(c_bcr_time+1)-1 downto 0) := (others => '0');
  signal r_adr     : std_logic_vector(g_bits-1 downto 1);
  signal s_adr     : std_logic_vector(g_bits-1 downto 1);

  -- Request FIFO interface
  signal r_pop     : std_logic;
  signal r_push    : std_logic;
  signal s_last    : std_logic;
  signal s_last_n  : std_logic; -- future state
  signal s_full_n  : std_logic;
  signal s_fo_dat  : std_logic_vector(ps_data'range);
  signal s_fo_sel  : std_logic_vector(ps_seln'range);

  -- Request FIFO state
  signal r_flags   : std_logic_vector(2 downto 0);
  signal s_flags   : std_logic_vector(2 downto 0);
  signal r_hi_dat  : std_logic_vector(slave_i.dat'range);
  signal r_lo_dat  : std_logic_vector(slave_i.dat'range);
  signal r_hi_sel  : std_logic_vector(slave_i.sel'range);
  signal r_lo_sel  : std_logic_vector(slave_i.sel'range);

  -- PSRAM output path
  signal r_ack     : std_logic;
  signal r_wbo_mux : std_logic;
  signal r_dati_l  : std_logic_vector(ps_data'range);
  signal r_dati_h  : std_logic_vector(ps_data'range);

begin

  -- Keep the PSRAM synchronous to the SoC bus for lower latency
  ps_clk        <= clk_i;
  slave_o.err   <= '0';
  slave_o.rty   <= '0';
  slave_o.ack   <= r_ack;
  slave_o.stall <= not r_push;
  slave_o.dat(31 downto 16) <= r_dati_h;
  slave_o.dat(15 downto  0) <= r_dati_l;

  fsm : process(clk_i, rstn_i) is
  begin
    if rstn_i = '0' then
      r_state <= S_RESET;
      r_count <= (others => '0');
    elsif rising_edge(clk_i) then
      case r_state is
        when S_RESET =>
          r_state <= S_BCR_REQUEST;

        when S_IDLE =>
          if (slave_i.cyc and slave_i.stb) = '1' then
            if slave_i.we = '1' then
              r_state <= S_WRITE_REQUEST;
            else
              r_state <= S_READ_REQUEST;
            end if;
          end if;

        when S_WRITE_REQUEST =>
          r_state <= S_WRITE_WAIT;

        when S_WRITE_WAIT => -- r_pop is not yet valid
          r_state <= S_WRITE_LATCH;

        when S_WRITE_LATCH =>
          if r_pop = '1' and s_last = '1' then
            r_state <= S_IDLE;
          end if;

        when S_READ_REQUEST =>
          r_state <= S_READ_WAIT;

        when S_READ_WAIT => -- r_pop is not yet valid
          r_state <= S_READ_LATCH;

        when S_READ_LATCH =>
          if r_pop = '1' and s_last = '1' then
            r_state <= S_IDLE;
          end if;

        when S_BCR_REQUEST =>
          r_state <= S_BCR_WAIT;

        when S_BCR_WAIT =>
          r_count <= r_count + 1;
          if r_count = c_bcr_time then
            r_state <= S_BCR_GAP;
          end if;

        when S_BCR_GAP =>
          r_state <= S_BCR_READ;

        when S_BCR_READ =>
          r_state <= S_IDLE;
      end case;
    end if;
  end process;

  -- Burst-mode has 8 word wrap-around (low 3 bits)
  s_adr(r_adr'high downto g_row_bits) <= r_adr(r_adr'high downto g_row_bits);
  s_adr(g_row_bits-1 downto 1) <= std_logic_vector(unsigned(r_adr(g_row_bits-1 downto 1)) + 1);

  -- Push requests from WB input the FIFO
  -- Accept any request in S_IDLE; otherwise, accept requests which continues the burst
  wb_input : process(clk_i, rstn_i) is
  begin
    if rstn_i = '0' then
      r_push <= '0';
      r_adr  <= (others => '0');
    elsif rising_edge(clk_i) then
      r_push <= '0';
      case r_state is
        when S_IDLE =>
          -- Consume first request
          r_adr <= slave_i.adr(g_bits downto 2);
          r_push <= slave_i.cyc and slave_i.stb;
        when S_WRITE_WAIT | S_WRITE_LATCH =>
          -- Don't push if last => could mean FIFO has something leftover when FSM goes to S_IDLE
          -- Don't push if full, obviously
          -- Don't push if we just pushed => need to inspect next WB request
          if (not s_last_n and not s_full_n and not r_push and slave_i.cyc and slave_i.stb and slave_i.we) = '1' and
             s_adr = slave_i.adr(g_bits downto 2) then
            r_push <= '1';
            r_adr  <= s_adr;
          end if;
        when S_READ_WAIT | S_READ_LATCH =>
          if (not s_last_n and not s_full_n and not r_push and slave_i.cyc and slave_i.stb and not slave_i.we) = '1' and
            s_adr = slave_i.adr(g_bits downto 2) then
            r_push <= '1';
            r_adr  <= s_adr;
          end if;
        when S_BCR_READ =>
          r_push <= '1';
        when others =>
          null;
      end case;
    end if;
  end process;

  -- 32:16 FIFO process, big-endian
  s_fo_dat <= r_lo_dat(31 downto 16);
  s_fo_sel <= r_lo_sel( 3 downto  2);
  s_last   <= '1' when r_flags = "001" else '0';
  s_last_n <= '1' when s_flags = "001" else '0';
  s_full_n <= s_flags(2);
  -- r_state: Hll
  --   H  = hi is full
  --   ll = 00:lo=empty, 01:lo=one, 11:lo=both
  fifo_regs : process(clk_i, rstn_i) is
  begin
    if rstn_i = '0' then
      r_flags  <= "000";
      r_hi_dat <= (others => '0');
      r_lo_dat <= (others => '0');
      r_hi_sel <= (others => '0');
      r_lo_sel <= (others => '0');
    elsif rising_edge(clk_i) then
      if r_push = '1' then
        r_hi_dat <= slave_i.dat;
        r_hi_sel <= slave_i.sel;
      end if;
      if r_pop = '1' or r_flags(0) = '0' then
        if r_flags(1) = '1' then
          r_lo_dat(31 downto 16) <= r_lo_dat(15 downto 0);
          r_lo_sel( 3 downto  2) <= r_hi_sel( 1 downto 0);
        else
          r_lo_dat <= r_hi_dat;
          r_lo_sel <= r_hi_sel;
        end if;
      end if;

      -- Report bad pushes
      assert (r_push = '0' or (r_flags(2) = '0' and r_flags /= "001"))
      report "Forbidden push" severity error;

      r_flags <= s_flags;
    end if;
  end process;

  fifo_fsm : process(r_flags, r_push, r_pop) is
  begin
    case r_flags is
      when "000" => -- pop undefined
        if r_push = '1' then
          s_flags <= "100";
        else
          s_flags <= "000";
        end if;
      when "001" => -- push forbidden
        if r_pop = '1' then
          s_flags <= "000";
        else
          s_flags <= "001";
        end if;
      when "011" =>
        if r_pop = '1' then
          if r_push = '1' then
            s_flags <= "101";
          else
            s_flags <= "001";
          end if;
        else
          if r_push = '1' then
            s_flags <= "111";
          else
            s_flags <= "011";
          end if;
        end if;
      when "100" => -- push impossible (S_*_WAIT) and pop undefined
        s_flags <= "011";
      when "101" => -- push forbidden
        if r_pop = '1' then
          s_flags <= "011";
        else
          s_flags <= "101";
        end if;
      when "111" => -- push forbidden
        if r_pop = '1' then
          s_flags <= "101";
        else
          s_flags <= "111";
        end if;
      when others => -- impossible state
        s_flags <= "000";
    end case;
  end process;

  -- Combine two 16-bit words into one 32-bit word
  wb_output : process(clk_i) is
  begin
    if rising_edge(clk_i) then
      r_wbo_mux <= '0';
      r_ack     <= '0';
      r_dati_h  <= (others => '0');
      case r_state is
        when S_WRITE_LATCH | S_READ_LATCH =>
          if r_pop = '1' then
            r_wbo_mux <= not r_wbo_mux;
            r_ack     <= r_wbo_mux;
            r_dati_h  <= r_dati_l;
          end if;
        when others =>
          null;
      end case;
    end if;
  end process;

  -- Simple fast input registers for good timing closure
  sram_input : process(clk_i) is
  begin
    if rising_edge(clk_i) then
      r_pop    <= ps_wait;
      r_dati_l <= ps_data;
    end if;
  end process;

  sram_output : process(clk_i, rstn_i) is
  begin
    if rstn_i = '0' then
      ps_addr <= (others => '0');
      ps_data <= (others => 'Z');
      ps_advn <= '1';
      ps_cen  <= '1';
      ps_seln <= (others => '0');
      ps_wen  <= '1';
      ps_oen  <= '1';
      ps_cre  <= '0';
    elsif falling_edge(clk_i) then
      case r_state is

        when S_IDLE | S_RESET | S_BCR_GAP =>
          ps_addr <= (others => '0');
          ps_data <= (others => 'Z');
          ps_advn <= '1';
          ps_cen  <= '1';
          ps_seln <= (others => '0');
          ps_wen  <= '1';
          ps_oen  <= '1';
          ps_cre  <= '0';

        when S_WRITE_REQUEST =>
          ps_addr <= r_adr & "0";
          ps_data <= r_adr(15 downto 1) & "0";
          ps_advn <= '0';
          ps_cen  <= '0';
          ps_seln <= (others => '0');
          ps_wen  <= '0';
          ps_oen  <= '1';
          ps_cre  <= '0';

        when S_WRITE_WAIT | S_WRITE_LATCH =>
          ps_addr <= (others => '0');
          ps_data <= s_fo_dat;
          ps_advn <= '1';
          ps_cen  <= '0';
          ps_seln <= not s_fo_sel;
          ps_wen  <= '0';
          ps_oen  <= '1';
          ps_cre  <= '0';

        when S_READ_REQUEST =>
          ps_addr <= r_adr & "0";
          ps_data <= r_adr(15 downto 1) & "0";
          ps_advn <= '0';
          ps_cen  <= '0';
          ps_seln <= (others => '0');
          ps_wen  <= '1';
          ps_oen  <= '1';
          ps_cre  <= '0';

        when S_READ_WAIT | S_READ_LATCH =>
          ps_addr <= (others => '0');
          ps_data <= (others => 'Z');
          ps_advn <= '1';
          ps_cen  <= '0';
          ps_seln <= (others => '0');
          ps_wen  <= '1';
          ps_oen  <= '0';
          ps_cre  <= '0';

        when S_BCR_REQUEST =>
          ps_addr <= c_bcr_setup;
          ps_data <= c_bcr_setup(ps_data'range);
          ps_advn <= '0';
          ps_cen  <= '0';
          ps_seln <= (others => '0');
          ps_wen  <= '1';
          ps_oen  <= '1';
          ps_cre  <= '1';

        when S_BCR_WAIT =>
          ps_addr <= c_bcr_setup;
          ps_data <= c_bcr_setup(ps_data'range);
          ps_advn <= '1';
          ps_cen  <= '0';
          ps_seln <= (others => '0');
          ps_wen  <= '0';
          ps_oen  <= '1';
          ps_cre  <= '1';

        when S_BCR_READ => -- read wherever
          ps_addr <= (others => '0');
          ps_data <= (others => 'Z');
          ps_advn <= '0';
          ps_cen  <= '0';
          ps_seln <= (others => '0');
          ps_wen  <= '1';
          ps_oen  <= '1';
          ps_cre  <= '0';

      end case;
    end if;
  end process;

end rtl;
