// ref_pll10.v

// Generated using ACDS version 18.1 625

`timescale 1 ps / 1 ps
module ref_pll10 (
		input  wire [4:0] cntsel,           //           cntsel.cntsel
		output wire [1:0] loaden,           //           loaden.loaden
		output wire       locked,           //           locked.export
		output wire [1:0] lvds_clk,         //         lvds_clk.lvds_clk
		input  wire [2:0] num_phase_shifts, // num_phase_shifts.num_phase_shifts
		output wire       outclk_2,         //          outclk2.clk
		output wire       outclk_3,         //          outclk3.clk
		output wire       outclk_4,         //          outclk4.clk
		output wire       phase_done,       //       phase_done.phase_done
		input  wire       phase_en,         //         phase_en.phase_en
		input  wire       refclk,           //           refclk.clk
		input  wire       rst,              //            reset.reset
		input  wire       scanclk,          //          scanclk.clk
		input  wire       updn              //             updn.updn
	);

	ref_pll10_altera_iopll_181_m5i74da iopll_0 (
		.rst              (rst),              //            reset.reset
		.refclk           (refclk),           //           refclk.clk
		.locked           (locked),           //           locked.export
		.scanclk          (scanclk),          //          scanclk.clk
		.phase_en         (phase_en),         //         phase_en.phase_en
		.updn             (updn),             //             updn.updn
		.cntsel           (cntsel),           //           cntsel.cntsel
		.phase_done       (phase_done),       //       phase_done.phase_done
		.num_phase_shifts (num_phase_shifts), // num_phase_shifts.num_phase_shifts
		.lvds_clk         (lvds_clk),         //         lvds_clk.lvds_clk
		.loaden           (loaden),           //           loaden.loaden
		.outclk_2         (outclk_2),         //          outclk2.clk
		.outclk_3         (outclk_3),         //          outclk3.clk
		.outclk_4         (outclk_4)          //          outclk4.clk
	);

endmodule
