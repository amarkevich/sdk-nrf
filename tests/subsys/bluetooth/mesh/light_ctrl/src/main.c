/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <ztest.h>
#include <bluetooth/mesh.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/light_ctrl_srv.h>

#define FLAGS_CONFIGURATION (BIT(FLAG_STARTED) | BIT(FLAG_OCC_MODE))

enum flags {
	FLAG_ON,
	FLAG_OCC_MODE,
	FLAG_MANUAL,
	FLAG_REGULATOR,
	FLAG_OCC_PENDING,
	FLAG_ON_PENDING,
	FLAG_OFF_PENDING,
	FLAG_TRANSITION,
	FLAG_STORE_CFG,
	FLAG_STORE_STATE,
	FLAG_CTRL_SRV_MANUALLY_ENABLED,
	FLAG_STARTED,
	FLAG_RESUME_TIMER,
};

/** Mocks ******************************************/

static struct bt_mesh_model mock_lightness_model = { .elem_idx = 0 };

static struct bt_mesh_lightness_srv lightness_srv = {
	.lightness_model = &mock_lightness_model
};

static struct bt_mesh_light_ctrl_srv light_ctrl_srv =
	BT_MESH_LIGHT_CTRL_SRV_INIT(&lightness_srv);

static struct bt_mesh_model mock_ligth_ctrl_model = {
	.user_data = &light_ctrl_srv,
	.elem_idx = 1
};

void lightness_srv_change_lvl(struct bt_mesh_lightness_srv *srv,
			      struct bt_mesh_msg_ctx *ctx,
			      struct bt_mesh_lightness_set *set,
			      struct bt_mesh_lightness_status *status,
			      bool publish)
{
	ztest_check_expected_value(srv);
	zassert_is_null(ctx, "Context was not null");
	ztest_check_expected_value(set->lvl);
	ztest_check_expected_data(set->transition,
				  sizeof(struct bt_mesh_model_transition));
	/* status can not be tested as it is a dummy value for one call that can be anything. */
	zassert_true(publish, "Publication not done");
}

int lightness_on_power_up(struct bt_mesh_lightness_srv *srv)
{
	zassert_not_null(srv, "Context was null");
	return 0;
}

uint8_t model_transition_encode(int32_t transition_time)
{
	return 0;
}

int model_send(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
	       struct net_buf_simple *buf)
{
	ztest_check_expected_value(model);
	ztest_check_expected_value(ctx);
	ztest_check_expected_value(buf->len);
	ztest_check_expected_data(buf->data, buf->len);
	return 0;
}

int bt_mesh_onoff_srv_pub(struct bt_mesh_onoff_srv *srv,
			  struct bt_mesh_msg_ctx *ctx,
			  const struct bt_mesh_onoff_status *status)
{
	ztest_check_expected_value(srv);
	zassert_is_null(ctx, "Context was not null");
	ztest_check_expected_value(status->present_on_off);
	ztest_check_expected_value(status->target_on_off);
	ztest_check_expected_value(status->remaining_time);
	return 0;
}

void bt_mesh_model_msg_init(struct net_buf_simple *msg, uint32_t opcode)
{
	ztest_check_expected_value(opcode);
	net_buf_simple_init(msg, 0);
}

int bt_mesh_model_extend(struct bt_mesh_model *mod,
			 struct bt_mesh_model *base_mod)
{
	return 0;
}

/** End Mocks **************************************/
static struct bt_mesh_model_transition expected_transition;
static struct bt_mesh_lightness_set expected_set = {
	.transition = &expected_transition
};
static uint8_t expected_msg[10];
static struct bt_mesh_onoff_status expected_onoff_status = { 0 };

static void expect_light_onoff_pub(uint8_t *expected_msg, size_t len)
{
	ztest_expect_value(bt_mesh_model_msg_init, opcode,
			   BT_MESH_LIGHT_CTRL_OP_LIGHT_ONOFF_STATUS);
	ztest_expect_value(model_send, model, &mock_ligth_ctrl_model);
	ztest_expect_value(model_send, ctx, NULL);
	ztest_expect_value(model_send, buf->len, len);
	ztest_expect_data(model_send, buf->data, expected_msg);
	ztest_expect_value(bt_mesh_onoff_srv_pub, srv, &light_ctrl_srv.onoff);
	ztest_expect_value(bt_mesh_onoff_srv_pub, status->present_on_off,
			   expected_onoff_status.present_on_off);
	ztest_expect_value(bt_mesh_onoff_srv_pub, status->target_on_off,
			   expected_onoff_status.target_on_off);
	ztest_expect_value(bt_mesh_onoff_srv_pub, status->remaining_time,
			   expected_onoff_status.remaining_time);
}

static void expect_transition_start(void)
{
	ztest_expect_value(lightness_srv_change_lvl, srv, &lightness_srv);
	ztest_expect_value(lightness_srv_change_lvl, set->lvl,
			   expected_set.lvl);
	ztest_expect_data(lightness_srv_change_lvl, set->transition,
			  expected_set.transition);
}

static void expect_ctrl_disable(void)
{
	expected_msg[0] = 0;
	expected_onoff_status.present_on_off = false;
	expected_onoff_status.target_on_off = false;
	expected_onoff_status.remaining_time = 0;
	expect_light_onoff_pub(expected_msg, 1);
}

static void expect_ctrl_enable(void)
{
	expected_transition.time = 0;
	expected_set.lvl = light_ctrl_srv.cfg.light[LIGHT_CTRL_STATE_STANDBY];
	expect_transition_start();
}

static void expect_turn_off_state_change(void)
{
	expected_transition.time = light_ctrl_srv.cfg.fade_standby_manual;
	expected_set.lvl = light_ctrl_srv.cfg.light[LIGHT_CTRL_STATE_STANDBY];
	expect_transition_start();

	expected_msg[0] = 1;
	expected_msg[1] = 0;
	expected_msg[2] = 0;
	expected_onoff_status.present_on_off = true;
	expected_onoff_status.target_on_off = false;
	expected_onoff_status.remaining_time = 510;
	expect_light_onoff_pub(expected_msg, 3);
}

static void expect_turn_on_state_change(void)
{
	expected_transition.time = light_ctrl_srv.cfg.fade_on;
	expected_set.lvl = light_ctrl_srv.cfg.light[LIGHT_CTRL_STATE_ON];
	expect_transition_start();

	expected_msg[0] = 1;
	expected_msg[1] = 1;
	expected_msg[2] = 0;
	expected_onoff_status.present_on_off = true;
	expected_onoff_status.target_on_off = true;
	expected_onoff_status.remaining_time = 510;
	expect_light_onoff_pub(expected_msg, 3);
}

static void
expected_statemachine_cond(bool enabled,
			   enum bt_mesh_light_ctrl_srv_state expected_state,
			   atomic_t expected_flags)
{
	if (enabled) {
		zassert_not_null(light_ctrl_srv.lightness->ctrl,
				 "Is not enabled.");
	} else {
		zassert_is_null(light_ctrl_srv.lightness->ctrl,
				"Is not disabled");
	}
	zassert_equal(light_ctrl_srv.state, expected_state, "Wrong state");
	zassert_equal(light_ctrl_srv.flags, expected_flags,
		      "Wrong Flags: 0x%X:0x%X", light_ctrl_srv.flags,
		      expected_flags);
}

static void test_fsm_no_change_by_light_onoff(void)
{
	/* The light ctrl server is disable after init, and in OFF state */
	enum bt_mesh_light_ctrl_srv_state expected_state =
		LIGHT_CTRL_STATE_STANDBY;
	atomic_t expected_flags = FLAGS_CONFIGURATION;

	expected_statemachine_cond(false, expected_state, expected_flags);

	bt_mesh_light_ctrl_srv_on(&light_ctrl_srv);
	expected_statemachine_cond(false, expected_state, expected_flags);

	bt_mesh_light_ctrl_srv_off(&light_ctrl_srv);
	expected_statemachine_cond(false, expected_state, expected_flags);

	/* Enable light ctrl server, to enter STANDBY state */
	expected_flags = expected_flags | BIT(FLAG_CTRL_SRV_MANUALLY_ENABLED);
	expected_flags = expected_flags | BIT(FLAG_TRANSITION);
	expect_ctrl_enable();
	bt_mesh_light_ctrl_srv_enable(&light_ctrl_srv);

	/* Wait for transition to completed. */
	expected_flags = expected_flags & ~BIT(FLAG_TRANSITION);
	k_sleep(K_MSEC(5)); // Sleep test to allow timeout to run.

	expected_statemachine_cond(true, expected_state, expected_flags);

	/* Light_Off should not change state or flags */
	bt_mesh_light_ctrl_srv_off(&light_ctrl_srv);
	expected_statemachine_cond(true, expected_state, expected_flags);

	/* Light_On should change state to FADE_ON state */
	expected_state = LIGHT_CTRL_STATE_ON;
	expected_flags = expected_flags | BIT(FLAG_ON);
	expected_flags = expected_flags | BIT(FLAG_TRANSITION);
	expected_flags = expected_flags & ~BIT(FLAG_MANUAL);
	expect_turn_on_state_change();
	bt_mesh_light_ctrl_srv_on(&light_ctrl_srv);
	expected_statemachine_cond(true, expected_state, expected_flags);

	/* Light_On should not change state or flags */
	bt_mesh_light_ctrl_srv_on(&light_ctrl_srv);
	expected_statemachine_cond(true, expected_state, expected_flags);

	/* Light_Off should change state to FADE_STANDBY_MANUAL state */
	expected_state = LIGHT_CTRL_STATE_STANDBY;
	expected_flags = expected_flags | BIT(FLAG_MANUAL);
	expected_flags = expected_flags & ~BIT(FLAG_ON);
	expect_turn_off_state_change();
	bt_mesh_light_ctrl_srv_off(&light_ctrl_srv);
	expected_statemachine_cond(true, expected_state, expected_flags);

	/* Light_Off should not change state or flags */
	bt_mesh_light_ctrl_srv_off(&light_ctrl_srv);
	expected_statemachine_cond(true, expected_state, expected_flags);
}

static void setup(void)
{
	zassert_not_null(_bt_mesh_light_ctrl_srv_cb.init, "Init cb is null");
	zassert_not_null(_bt_mesh_light_ctrl_srv_cb.start, "Start cb is null");

	zassert_ok(_bt_mesh_light_ctrl_srv_cb.init(&mock_ligth_ctrl_model),
		   "Init failed");
	expect_ctrl_disable();
	zassert_ok(_bt_mesh_light_ctrl_srv_cb.start(&mock_ligth_ctrl_model),
		   "Start failed");
}

static void teardown(void)
{
	zassert_not_null(_bt_mesh_light_ctrl_srv_cb.reset, "Reset cb is null");
	expect_ctrl_disable();
	_bt_mesh_light_ctrl_srv_cb.reset(&mock_ligth_ctrl_model);
}

void test_main(void)
{
	ztest_test_suite(light_ctrl_test,
			 ztest_unit_test_setup_teardown(
				 test_fsm_no_change_by_light_onoff, setup,
				 teardown));

	ztest_run_test_suite(light_ctrl_test);
}
