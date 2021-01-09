

func(this *User) Marshal_V1()([]byte,error) {
	//ToDo 测试修改
	returen proto.Marshal(this)
}

func (this *User)Unmarshal_V1()(*User,error){
	return nil,nil
}


func(this *User)Marshal_V2()([]byte,error){
	return proto.Marshal(this)
}


func (this *User)Unmarshal_V2()(*User,error){
	return nil,nil
}

