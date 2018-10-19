#include "DSAGenerator.h"
#include <vector>

using namespace llvm;

DIType *DSAGenerator::getBaseType(DIType *Ty)
{
    if (Ty->getTag() == dwarf::DW_TAG_pointer_type ||
        Ty->getTag() == dwarf::DW_TAG_member ||
        Ty->getTag() == dwarf::DW_TAG_typedef)
    {
        DIType *baseTy = dyn_cast<DIDerivedType>(Ty)->getBaseType().resolve();
        if (!baseTy) {
            errs() << "Type : NULL - Nothing more to do\n";
            return NULL;
        }
        return baseTy;
    }
    return Ty;
}

DIType *DSAGenerator::getLowestDINode(DIType *Ty)
{
    if (Ty->getTag() == dwarf::DW_TAG_pointer_type ||
        Ty->getTag() == dwarf::DW_TAG_member ||
        Ty->getTag() == dwarf::DW_TAG_typedef)
    {
        DIType *baseTy = dyn_cast<DIDerivedType>(Ty)->getBaseType().resolve();
        if (!baseTy) {
            errs() << "Type : NULL - Nothing more to do\n";
            return NULL;
        }

        //Skip all the DINodes with DW_TAG_typedef tag
        while ((baseTy->getTag() == dwarf::DW_TAG_typedef ||
                baseTy->getTag() == dwarf::DW_TAG_const_type ||
                baseTy->getTag() == dwarf::DW_TAG_pointer_type)) {

            if (DITypeRef temp = dyn_cast<DIDerivedType>(baseTy)->getBaseType())
                baseTy = temp.resolve();
            else
                break;
        }
        return baseTy;
    }
    return Ty;
}

std::string DSAGenerator::getStructName(DIType *Ty) {
    DIType* baseTy = getLowestDINode(Ty);
    if (!baseTy)
    {
        return "";
    }

    if (baseTy->getTag() == dwarf::DW_TAG_structure_type)
    {
        return baseTy->getName().str();
    }

    return "";
}

int DSAGenerator::getAllNames(DIType *Ty, std::set<std::string> seen_names, offsetNames &of, int visit_order, std::string baseName, std::string indent, StringRef argName, std::string &structName)
{
    std::string printinfo = moduleName + "[getAllNames]: ";
    //errs() << rootDer->getName().str() << "\n";
    DIType *baseTy = getLowestDINode(Ty);
    if (!baseTy)
        return 0;
    // If that pointer is a struct
    if (baseTy->getTag() == dwarf::DW_TAG_structure_type)
    {
        errs() << "Find structure type\n";
        structName = baseTy->getName().str();
        errs() << "Current struct name: " << structName << "\n";
        DICompositeType *compType = dyn_cast<DICompositeType>(baseTy);
        // Go thro struct elements and print them all
        //std::string curStructName; 
        //of[visit_order] = std::pair<std::string, DIType *>( new_name, der->getBaseType().resolve());
        for (DINode *Op : compType->getElements())
        {
            //visit_order += 1;
            DIDerivedType *der = dyn_cast<DIDerivedType>(Op);
            unsigned offset = der->getOffsetInBits() >> 3;

            // do some type checking. If recursive type call, return
            std::string curFieldName = der->getName().str();
            if (seen_names.find(curFieldName) != seen_names.end()) {
                errs() << "Find repeat struct name. Break Here!"  << "\n";
                continue;
            }
            seen_names.insert(curFieldName);
            std::string new_name(baseName);
            if (new_name != "") new_name.append(".");
            new_name.append(curFieldName);
            errs() << "A new sturct name: " << curFieldName << "\n";

            errs() << printinfo << "type information:  " << der->getBaseType().resolve()->getTag() << "\n";
            errs() << printinfo << "Updating [of] on line 192 with following pair:\n";
            errs() << printinfo << "first item [new_name] " << new_name << " - " << visit_order << "\n";
            //visit_order += 1;
            of[visit_order] = std::pair<std::string, DIType *>( new_name, der->getBaseType().resolve());
            /// XXX: crude assumption that we want to peek only into those members
            /// whose sizes are greater than 8 bytes
            if (((der->getSizeInBits() >> 3) > 1)
                && der->getBaseType().resolve()->getTag()) {
                std::string tempStructName("");
                errs() << printinfo <<"RECURSIVELY CALL getAllNames on 200\n";
                visit_order = getAllNames(dyn_cast<DIType>(der), seen_names, of, visit_order + 1,
                            new_name, indent, argName, tempStructName);
            }
            errs() << "--------------- " << der->getName().str() << "\n";
        }
    }
    else if (isa<DIBasicType>(baseTy))
    {
        // here, getting member type instead of pointer type.
        DIType* oneLevelUpBaseType = getBaseType(Ty);
        if ( oneLevelUpBaseType->getTag() == dwarf::DW_TAG_pointer_type ) // class member type ... 
        {
            errs() << "Adding 2 offset" << "\n";
            visit_order += 1;
        }

        structName = "";
        //structName = baseTy->getName().str();
        //of[visit_order] = std::pair<std::string, DIType *>( new_name, der->getBaseType().resolve());
        //if type tag for the parameter is of pointer_type and DI type is DIBasicType
        //then treat it as a pointer of native type
        errs()<< printinfo << "Updating [of] on line 209 with following pair:\n";
        errs()<< printinfo << "first item [argName.str()] "<<argName.str()<<"\n";
        // of[0] = std::pair<std::string, DIType *>(argName.str(), bas);//This may be overwriting the correct name of the first member of the structure.
        //TODO need to see whether this case (this else if situation) needs to be handled at all.
    }
    else
    {
        errs() << "Find unknown types\n";
        structName = "";
        // same here, unkonwn type could be function pointer etc. In this case, we assume it will have a child created
    }
    return visit_order;
}

DSAGenerator::offsetNames DSAGenerator::getArgFieldNames(Function *F, unsigned argNumber, StringRef argName, std::string &structName)
{
    std::string printinfo = moduleName + "[getArgFieldNames]: ";
    offsetNames offNames;
    //didn't find any such case
    if (argNumber > F->arg_size()) {
        errs() << printinfo << "### WARN : requested data for non-existent element\n";
        return offNames;
    }

    SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
    F->getAllMetadata(MDs);
    for (auto &MD : MDs)
    {
        if (MDNode *N = MD.second)
        {
            if (DISubprogram *subprogram = dyn_cast<DISubprogram>(N))
            {
                auto *subRoutine = subprogram->getType();
                //XXX:if a function takes in no arguments, how can we assume it is of type void?
                if (!subRoutine->getTypeArray()[0])
                {
                    errs() << printinfo << "return type \"void\" for Function : " << F->getName().str() << "\n";
                }

                const auto &TypeRef = subRoutine->getTypeArray();

                /// XXX: When function arguments are coerced in IR, the corresponding
                /// debugInfo extracted for that function from the source code will
                /// not have the same number of arguments. Check the indexes to
                /// prevent array out of bounds exception (segfault)

                //did not encounter this case with dummy.c
                if (argNumber >= TypeRef.size())
                {
                    errs() << printinfo << "TypeArray request out of bounds. Are parameters coerced??\n";
                    goto done;
                }

                if (const auto &ArgTypeRef = TypeRef[argNumber])
                {
                    // Resolve the type
                    DIType *Ty = ArgTypeRef.resolve();
                    // Handle Pointer type
                    if (F->getName() == "passF")
                        errs() << "BEGIN WATCH\n";
                    errs() << printinfo << "CALL getAllNames on line 266 with these params:\n";
                    errs() << printinfo << "argName = " << argName << "\n";
                    std::string baseStructName = getStructName(Ty);

                    if (seenStructs.find(baseStructName) != seenStructs.end())
                    {
                        offNames = seenStructs[baseStructName];
                        continue;
                    }

                    std::set<std::string> seen_names;
                    //getAllNames(Ty, seen_names, offNames, 0, "", "  ", argName, structName);
                    getAllNames(Ty, seen_names, offNames, 1, "", "  ", argName, structName);
                    seen_names.clear();
                    seenStructs[structName] = offNames;
                    errs() << printinfo << "structName = " << structName << "\n";
                }
            }
        }
    }

done:
    return offNames;
}

std::vector<DbgDeclareInst *> DSAGenerator::getDbgDeclareInstInFunction(Function *F)
{
    std::vector<DbgDeclareInst *> dbg_decl_inst;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
    {
        if (!isa<CallInst>(*I))
        {
            continue;
        }
        Instruction *pInstruction = dyn_cast<Instruction>(&*I);
        if (DbgDeclareInst *ddi = dyn_cast<DbgDeclareInst>(pInstruction)) {
            dbg_decl_inst.push_back(&*ddi);
        }
    }
    return dbg_decl_inst;
}

std::vector<MDNode*> DSAGenerator::getParameterNodeInFunction(Function *F) {
    std::vector<MDNode*> mdnodes;
    std::vector<DbgDeclareInst *> dbg_decl_insts = getDbgDeclareInstInFunction(F);
    for (DbgDeclareInst *ddi : dbg_decl_insts) {
        //DILocalVariable *div = ddi->getVariable();
        if (MDNode *MD = dyn_cast<MDNode>(ddi->getRawExpression())) {
            //if (MDNode *MD = dyn_cast<MDNode>(ddi->getRawVariable())) {
            mdnodes.push_back(MD);
        }
    }

    return mdnodes;
}

void DSAGenerator::dumpOffsetNames(offsetNames &of) {
    std::string printinfo = moduleName + "[dumpOffsetNames]: ";
    errs() << printinfo<<"Entered function\n";
    for (auto off : of) {
        errs() << printinfo<< "offset : " << off.first << "\n";
        errs() << printinfo<< "name : " << std::get<0>(off.second) << "\n";
        if (std::get<0>(off.second)=="Block") errs()<<"END WATCH\n";
    }
}

bool DSAGenerator::runOnModule(Module &M) {
    std::set<std::string> funcList = {
            "___might_sleep",
            "__alloc_percpu",
            "__rtnl_link_register",
            "__rtnl_link_unregister",
            "_cond_resched",
            "alloc_netdev_mqs",
            "consume_skb",
            "dummy_change_carrier",
            "dummy_cleanup_module",
            "dummy_dev_init",
            "dummy_dev_uninit",
            "dummy_get_drvinfo",
            "dummy_get_stats64",
            "dummy_init_module",
            "dummy_init_one",
            "dummy_setup",
            "dummy_validate",
            "dummy_xmit",
            "eth_hw_addr_random",
            "eth_mac_addr",
            "eth_random_addr",
            "eth_validate_addr",
            "ether_setup",
            "free_netdev",
            "free_percpu",
            "get_random_bytes",
            "is_multicast_ether_addr",
            "is_valid_ether_addr",
            "is_zero_ether_addr",
            "netif_carrier_off",
            "netif_carrier_on",
            "nla_data",
            "nla_len",
            "register_netdevice",
            "rtnl_link_unregister",
            "rtnl_lock",
            "rtnl_unlock",
            "set_multicast_list",
            "u64_stats_fetch_begin_irq",
            "u64_stats_fetch_retry_irq",
            "u64_stats_update_begin",
            "u64_stats_update_end"
    };

    for (Module::iterator FF = M.begin(), E = M.end(); FF != E; ++FF) {
        std::string printinfo = moduleName + "[runOnModule]: ";
        // collect dgb inst for
        Function *F = dyn_cast<Function>(FF);

// #ifdef TEST_IDL 
//         if (funcList.find(F->getName()) == funcList.end()) {
//             continue;
//         }
// #endif

        if (F->isDeclaration()) {
            continue;
        }

        std::map<unsigned, offsetNames> argNoToFieldNames;
        // iterate dgb and
        for (Argument &arg : F->args()) {
            if (arg.getType()->isPointerTy()) {
                std::string structName;
//#ifdef TEST_IDL
                errs()<<printinfo << "arg.getArgNo(){ "<<arg.getArgNo()<<" }\n";
                errs() << printinfo<<"CALL getArgFieldNames on line 521 with these parameters:\n";
                errs() << printinfo<<"F, s.t F.getName() = "<<F->getName()<<"\n";
                errs() << printinfo<<"arg.getArgNo() + 1 = "<<arg.getArgNo() + 1<<"\n";
                errs() << printinfo<<"arg.getName() = "<<arg.getName()<<"\n";
//#endif
                offsetNames of = getArgFieldNames(F, arg.getArgNo() + 1, arg.getName(), structName);
                //funcArgOffsetMap[F] = of;
                argNoToFieldNames[arg.getArgNo()] = of;
//#ifdef TEST_IDL
                errs() << printinfo<< "structName = " << structName <<"\n";
                errs() << printinfo<<"CALL dumpOffsetNames on line 523\n";
//#endif
                //dumpOffsetNames(of);
            }
        }

        funcArgOffsetMap[F] = argNoToFieldNames;
    }

    return false;
}

char DSAGenerator::ID = 0;
static RegisterPass<DSAGenerator> DSAGenerator("dsa-gen", "DSA struct field name generation for kernel", false, true);