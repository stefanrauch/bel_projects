target = "altera"
action = "synthesis"

fetchto = "../../../ip_cores"
syn_tool = "quartus"
syn_grade = "e1hg"
syn_package = "29"
syn_device = "10ax027e2f"
syn_top = "scu_control"
syn_project = "scu_control"
syn_family = "Arria 10"

quartus_preflow = "scu_control.tcl"

modules = {
  "local" : [
    "../../../top/gsi_scu/control4",
  ]
}
