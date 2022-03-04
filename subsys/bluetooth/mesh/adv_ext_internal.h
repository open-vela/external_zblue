/* bt_mesh_ext_adv START */
struct bt_mesh_ext_adv * const _bt_mesh_ext_adv_list[] =
{
#if defined(CONFIG_BT_MESH)
#if defined(CONFIG_BT_MESH_ADV_EXT)
	&adv_main,
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 1
	&adv_relay[0],
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 2
	&adv_relay[1],
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 3
	&adv_relay[2],
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 4
	&adv_relay[3],
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 5
	&adv_relay[4],
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 6
	&adv_relay[5],
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 7
	&adv_relay[6],
#if CONFIG_BT_MESH_RELAY_ADV_SETS >= 8
	&adv_relay[7],
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 8 */
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 7 */
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 6 */
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 5 */
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 4 */
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 3 */
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 2 */
#endif /* CONFIG_BT_MESH_RELAY_ADV_SETS >= 1 */
#if defined(CONFIG_BT_MESH_ADV_EXT_GATT_SEPARATE)
	&adv_gatt,
#endif /* CONFIG_BT_MESH_ADV_EXT_GATT_SEPARATE */
#endif /* CONFIG_BT_MESH_ADV_EXT */
#endif /* CONFIG_BT_MESH */
	NULL,
};
/* bt_mesh_ext_adv END */
