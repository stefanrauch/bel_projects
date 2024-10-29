-------------------------------------------------------------------------------
-- Title      : Test slave
-- Project    : all Arria platforms
-------------------------------------------------------------------------------
-- File       : virtualRAM.vhd
-- Author     : Lucas Herfurth
-- Company    : GSI
-- Created    : 2024-09-04
-- Last update: 2024-09-04
-- Platform   : Altera
-- Standard   : VHDL'93
-------------------------------------------------------------------------------
-- Description: Virtual RAM module to test DMA.
--
--
-------------------------------------------------------------------------------
--
-- Copyright (c) 2024 GSI / Lucas Herfurth
--
-- This source file is free software; you can redistribute it
-- and/or modify it under the terms of the GNU Lesser General
-- Public License as published by the Free Software Foundation;
-- either version 2.1 of the License, or (at your option) any
-- later version.
--
-- This source is distributed in the hope that it will be
-- useful, but WITHOUT ANY WARRANTY; without even the implied
-- warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
-- PURPOSE.  See the GNU Lesser General Public License for more
-- details.
--
-- You should have received a copy of the GNU Lesser General
-- Public License along with this source; if not, download it
-- from http://www.gnu.org/licenses/lgpl-2.1.html

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;
use work.gencores_pkg.all;
use work.genram_pkg.all;

entity virtualRAM is
  generic (
    g_size    : natural;
    g_nr_rams : natural
  );

  port(
    clk_sys_i : in std_logic;
    rst_n_i   : in std_logic;

    slave_i : in  t_wishbone_slave_in;
    slave_o : out t_wishbone_slave_out
  );
end entity;

architecture rtl of virtualRAM is

  signal ram_sel  : std_logic;
  signal slave0_i : t_wishbone_slave_in;
  signal slave0_o : t_wishbone_slave_out;
  signal slave1_i : t_wishbone_slave_in;
  signal slave1_o : t_wishbone_slave_out;

begin

  --solve this parametrically?
  --tests needed!
  ram_sel <= slave_i.adr(f_log2_size(g_size*4)+1);

  slave0_i <= slave_i when ram_sel = '0' else c_DUMMY_WB_SLAVE_IN;
  -- slave0_i.adr <= slave_i.adr(13 downto 0) when ram_sel = '0' else c_DUMMY_WB_SLAVE_IN; -- truncate address to avoid mirroring data from other RAMs !NOT NECESSARY IF DONE PROPERLY/ENOUGH ADDRESS SPACE IS RESERVED
  slave1_i <= slave_i when ram_sel = '1' else c_DUMMY_WB_SLAVE_IN;

  process (ram_sel)
  begin
    case ram_sel is
      when '0' =>
        slave_o <= slave0_o;
      when '1' =>
        slave_o <= slave1_o;
    end case;
  end process;

  RAM1 : xwb_dpram
  generic map (
    g_size                  => g_size,
    g_init_file             => "sw/main.ram",
    g_must_have_init_file   => true,
    g_slave1_interface_mode => CLASSIC,
    g_slave1_granularity    => BYTE
  )

  port map (
    clk_sys_i => clk_sys_i,
    rst_n_i   => rst_n_i,

    slave1_i => slave0_i,
    slave1_o => slave0_o,
    slave2_i => c_DUMMY_WB_SLAVE_IN,
    slave2_o => open
  );


  RAM2 : xwb_dpram
  generic map (
  g_size                  => g_size,
    g_must_have_init_file   => false,
    g_slave1_interface_mode => CLASSIC,
    g_slave1_granularity    => BYTE
  )

  port map (
    clk_sys_i => clk_sys_i,
    rst_n_i   => rst_n_i,

    slave1_i => slave1_i,
    slave1_o => slave1_o,
    slave2_i => c_DUMMY_WB_SLAVE_IN,
    slave2_o => open
  );

end architecture;