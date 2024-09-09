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
    g_size  : natural
  );

  port(
    clk_sys_i : in std_logic;
    rst_n_i   : in std_logic;

    slave_i : in  t_wishbone_slave_in;
    slave_o : out t_wishbone_slave_out
  );
end entity;

architecture rtl of virtualRAM is

  signal dummy : t_wishbone_slave_out;

begin

  RAM : xwb_dpram
  generic map (
    g_size => g_size,
    g_must_have_init_file => false
  )

  port map (
    clk_sys_i => clk_sys_i,
    rst_n_i   => rst_n_i,

    slave1_i => slave_i,
    slave1_o => slave_o,
    slave2_i => c_DUMMY_WB_SLAVE_IN,
    slave2_o => dummy
  );

end architecture;