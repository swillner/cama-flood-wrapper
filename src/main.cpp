extern "C" {
void init_inputnam_mod_mp_init_inputnam_();
void init_map_mod_mp_init_map_();
void init_topo_mod_mp_init_topo_();
void init_cond_mod_mp_init_cond_();
void init_time_mod_mp_init_time_();
void control_tstp_mod_mp_control_tstp_();
void calc_fldstg_mod_mp_calc_fldstg_();
void additional_mod_mp_cleanup_tstp_();
void additional_mod_mp_restart_init_();
extern int mod_input_mp_irestart_;
extern int additional_mod_mp_ifirstin_;
}

int main() {
  init_inputnam_mod_mp_init_inputnam_();
  additional_mod_mp_ifirstin_ = 0;
  mod_input_mp_irestart_ = 2;
  init_map_mod_mp_init_map_();
  init_topo_mod_mp_init_topo_();
  init_cond_mod_mp_init_cond_();
  init_time_mod_mp_init_time_();
  control_tstp_mod_mp_control_tstp_();
  for (int i = 0; i < 4; ++i) {
      additional_mod_mp_cleanup_tstp_();
      additional_mod_mp_restart_init_();
      additional_mod_mp_ifirstin_ = 0;
      init_time_mod_mp_init_time_();
      control_tstp_mod_mp_control_tstp_();
  }
  return 0;
}
