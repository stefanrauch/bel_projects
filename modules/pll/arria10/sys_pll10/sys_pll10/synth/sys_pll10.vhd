-- sys_pll10.vhd

-- Generated using ACDS version 18.1 625

library IEEE;
library sys_pll10_altera_iopll_181;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;
use sys_pll10_altera_iopll_181.sys_pll10_pkg.all;

entity sys_pll10 is
	port (
		locked   : out std_logic;        --  locked.export
		outclk_0 : out std_logic;        -- outclk0.clk
		outclk_1 : out std_logic;        -- outclk1.clk
		outclk_2 : out std_logic;        -- outclk2.clk
		outclk_3 : out std_logic;        -- outclk3.clk
		outclk_4 : out std_logic;        -- outclk4.clk
		refclk   : in  std_logic := '0'; --  refclk.clk
		rst      : in  std_logic := '0'  --   reset.reset
	);
end entity sys_pll10;

architecture rtl of sys_pll10 is
begin

	iopll_0 : component sys_pll10_altera_iopll_181.sys_pll10_pkg.sys_pll10_altera_iopll_181_cz2c3oa
		port map (
			rst      => rst,      --   reset.reset
			refclk   => refclk,   --  refclk.clk
			locked   => locked,   --  locked.export
			outclk_0 => outclk_0, -- outclk0.clk
			outclk_1 => outclk_1, -- outclk1.clk
			outclk_2 => outclk_2, -- outclk2.clk
			outclk_3 => outclk_3, -- outclk3.clk
			outclk_4 => outclk_4  -- outclk4.clk
		);

end architecture rtl; -- of sys_pll10
