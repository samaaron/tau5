-module(sp_midi).
-export([is_nif_loaded/0, is_nif_initialized/0, init/0, midi_init/0, midi_deinit/0, midi_send/2, midi_flush/0, midi_ins/0, midi_outs/0, have_my_pid/0,
        set_this_pid/1, set_log_level/1, get_current_time_microseconds/0, midi_refresh_devices/0]).

-define(APPLICATION, tau5).
-define(LIBNAME, "libsp_midi").

init() ->
    SoName = filename:join([code:priv_dir(?APPLICATION), "nif", ?LIBNAME]),
    erlang:load_nif(SoName, 0).

is_nif_loaded() ->
    false.
is_nif_initialized() ->
    false.
midi_init() ->
    done.
midi_deinit() ->
    done.
midi_send(_, _) ->
    done.
midi_flush() ->
    done.
midi_ins() ->
    [].
midi_outs() ->
    [].
have_my_pid() ->
    done.
get_current_time_microseconds() ->
    0.
set_log_level(_) ->
    done.
set_this_pid(_) ->
    done.
midi_refresh_devices() ->
    done.
