/* bt_mesh_ext_adv START */

#define  RELAY_ADV(i, _) &adv_relay[i]
struct bt_mesh_ext_adv * const _bt_mesh_ext_adv_list[] = {
	&adv_main,
#if CONFIG_BT_MESH_RELAY_ADV_SETS
	LISTIFY(CONFIG_BT_MESH_RELAY_ADV_SETS, RELAY_ADV, (,)),
#endif
#if defined(CONFIG_BT_MESH_ADV_EXT_GATT_SEPARATE)
	&adv_gatt,
#endif /* CONFIG_BT_MESH_ADV_EXT_GATT_SEPARATE */
	NULL,
};

/* bt_mesh_ext_adv END */
