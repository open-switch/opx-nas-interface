#!/usr/bin/python
import cps
import cps_object
import cps_utils

def test_get_ether_stat():
  cps_obj = cps_object.CPSObject(module = "dell-base-if-cmn/if/interfaces-state/interface/statistics", qual="observed")
  cps_obj.add_attr('if/interfaces-state/interface/name', "e101-001-0")
  ret_list = []
  print "Input object: ", cps_obj.get()
  assert cps.get([cps_obj.get()], ret_list)
  for ret in ret_list:
    o = cps_object.CPSObject(obj=ret)
    cps_utils.print_obj(o.get())

